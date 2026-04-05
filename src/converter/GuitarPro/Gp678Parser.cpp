#include "Gp678Parser.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <zlib.h>

// ============================================================
// XmlNode — ported from GP6File.cs Node class
// ============================================================

XmlNode* XmlNode::getSubnodeByName(const std::string& searchName, bool directOnly) const {
    if (name == searchName) return const_cast<XmlNode*>(this);
    if (directOnly) {
        for (const auto& n : subnodes) {
            if (n->name == searchName) return n.get();
        }
        return nullptr;
    }
    for (const auto& n : subnodes) {
        auto* result = n->getSubnodeByName(searchName);
        if (result) return result;
    }
    return nullptr;
}

XmlNode* XmlNode::getSubnodeByProperty(const std::string& propName, const std::string& propValue) const {
    for (const auto& n : subnodes) {
        for (size_t i = 0; i < n->propertyNames.size(); i++) {
            if (n->propertyNames[i] == propName && i < n->propertyValues.size() &&
                n->propertyValues[i] == propValue) {
                return n.get();
            }
        }
    }
    return nullptr;
}

// ============================================================
// BitStream — ported from GP6File.cs BitStream class
// ============================================================

GpBitStream::GpBitStream(const std::vector<uint8_t>& data) : data_(data) {}

bool GpBitStream::getBit() {
    if (finished_) return false;
    bool result = (data_[pointer_] >> (7 - subpointer_)) % 2 == 1;
    increaseSubpointer();
    return result;
}

std::vector<bool> GpBitStream::getBits(int amount) {
    std::vector<bool> result(amount);
    for (int i = 0; i < amount; i++) result[i] = getBit();
    return result;
}

uint8_t GpBitStream::getByte() {
    static const int powers_rev[] = {128, 64, 32, 16, 8, 4, 2, 1};
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<uint8_t>(getBit() ? powers_rev[i] : 0);
    }
    return result;
}

int GpBitStream::getBitsLE(int amount) {
    static const int powers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    int result = 0;
    for (int i = 0; i < amount; i++) {
        result |= getBit() ? powers[i] : 0;
    }
    return result;
}

int GpBitStream::getBitsBE(int amount) {
    static const int powers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    int result = 0;
    for (int i = 0; i < amount; i++) {
        result |= getBit() ? powers[amount - i - 1] : 0;
    }
    return result;
}

void GpBitStream::skipBits(int bits) {
    for (int i = 0; i < bits; i++) increaseSubpointer();
}

void GpBitStream::skipBytes(int bytes) {
    pointer_ += bytes;
}

void GpBitStream::increaseSubpointer() {
    subpointer_++;
    if (subpointer_ == 8) { subpointer_ = 0; pointer_++; }
    if (pointer_ >= static_cast<int>(data_.size())) finished_ = true;
}

// ============================================================
// GP6 Parser — ported from GP6File.cs
// ============================================================

Gp6Parser::Gp6Parser(const std::vector<uint8_t>& data) : rawData_(data) {}
Gp6Parser::Gp6Parser(const std::string& xml) : xmlContent_(xml), isXmlDirect_(true) {}

void Gp6Parser::readSong() {
    std::string xml;
    if (isXmlDirect_) {
        xml = xmlContent_;
    } else {
        xml = decompressGPX(rawData_);
    }

    auto root = parseGP6(xml);
    gp6NodeToGP5File(root.get());
}

std::string Gp6Parser::decompressGPX(const std::vector<uint8_t>& data) {
    // GPX files use BCFZ — a custom bit-level LZ77 compression
    if (data.size() < 8) throw std::runtime_error("GP6: file too small");

    // Check for BCFZ header
    if (data[0] == 'B' && data[1] == 'C' && data[2] == 'F' && data[3] == 'Z') {
        // BCFZ compressed format: 4-byte magic + 4-byte estimated size + LZ77 bitstream
        GpBitStream bs(data);
        bs.skipBytes(8); // Skip "BCFZ" + 4-byte size

        std::vector<uint8_t> decompressed;
        decompressed.reserve(data.size() * 4);

        while (!bs.isFinished()) {
            bool isCompressed = bs.getBit();

            if (isCompressed) {
                int wordSize = bs.getBitsBE(4);
                int offset = bs.getBitsLE(wordSize);
                int length = bs.getBitsLE(wordSize);

                int sourcePosition = static_cast<int>(decompressed.size()) - offset;
                if (sourcePosition < 0) break;

                int toRead = std::min(length, offset);
                for (int r = sourcePosition; r < sourcePosition + toRead; r++) {
                    decompressed.push_back(decompressed[r]);
                }
            } else {
                int byteLength = bs.getBitsLE(2);
                for (int x = 0; x < byteLength; x++) {
                    decompressed.push_back(bs.getByte());
                }
            }
        }

        // Search for XML content in decompressed BCF data
        std::string content(decompressed.begin(), decompressed.end());
        size_t xmlPos = std::string::npos;

        // Look for XML start: "<?xml" or "<GPIF"
        xmlPos = content.find("<?xml");
        if (xmlPos == std::string::npos) xmlPos = content.find("<GPIF");
        if (xmlPos == std::string::npos) {
            // Scan byte by byte for "<GPI"
            for (size_t i = 0; i + 3 < decompressed.size(); i++) {
                if (decompressed[i] == '<' && decompressed[i+1] == 'G' &&
                    decompressed[i+2] == 'P' && decompressed[i+3] == 'I') {
                    xmlPos = i;
                    break;
                }
            }
        }
        if (xmlPos != std::string::npos) {
            // Find end of XML
            auto endPos = content.rfind("</GPIF>");
            if (endPos != std::string::npos) {
                return content.substr(xmlPos, endPos - xmlPos + 7);
            }
            return content.substr(xmlPos);
        }

        throw std::runtime_error("GP6: could not find XML in decompressed data");
    }

    // Maybe it's already XML?
    std::string content(data.begin(), data.end());
    if (content.find("<GPIF") != std::string::npos || content.find("<?xml") != std::string::npos) {
        return content;
    }

    throw std::runtime_error("GP6: unrecognized file format");
}

std::unique_ptr<XmlNode> Gp6Parser::parseGP6(const std::string& xml, int start) {
    // Remove '<' chars inside CDATA tags
    std::string xmlCopy = xml;
    bool skipMode = false;
    for (size_t x = 0; x + 3 < xmlCopy.size(); x++) {
        std::string sub = xmlCopy.substr(x, 3);
        if (sub == "<!-") { xmlCopy[x] = '{'; continue; }
        if (sub == "<![") { skipMode = true; continue; }
        if (sub == "]]>") skipMode = false;
        if (skipMode && xmlCopy[x] == '<') xmlCopy[x] = '{';
    }

    // Split by '<'
    std::vector<std::string> split;
    std::string remaining = xmlCopy.substr(start);
    size_t pos = 0;
    while (pos < remaining.size()) {
        size_t nextLt = remaining.find('<', pos);
        if (nextLt == std::string::npos) {
            split.push_back(remaining.substr(pos));
            break;
        }
        split.push_back(remaining.substr(pos, nextLt - pos));
        pos = nextLt + 1;
    }

    std::vector<XmlNode*> stack;
    auto mainNode = std::make_unique<XmlNode>("");
    stack.push_back(mainNode.get());

    for (size_t x = 1; x < split.size(); x++) {
        const auto& s = split[x];
        if (s.empty()) continue;

        if (s[0] == '/') {
            // Closing tag
            if (stack.size() > 1) {
                auto* child = stack.back();
                stack.pop_back();
                // child is already owned by unique_ptr in parent's subnodes
                (void)child;
            }
            continue;
        }
        if (s.substr(0, 2) == "![") {
            // CDATA — already handled
            continue;
        }
        if (s[0] == '?') continue; // XML declaration

        // Normal tag
        size_t endOfTag = s.find('>');
        if (endOfTag == std::string::npos) break;

        size_t firstSpace = s.find(' ');
        size_t firstSlash = s.find('/');
        if (firstSpace == std::string::npos || firstSpace > endOfTag) firstSpace = endOfTag;
        if (firstSlash != std::string::npos && firstSlash < firstSpace) firstSpace = firstSlash;

        std::string tagName = s.substr(0, firstSpace);

        bool isSingleTag = false;
        std::vector<std::string> propNames, propValues;

        // Parse attributes
        size_t p = firstSpace;
        bool collectingValue = false;
        std::string propName, propValue;
        while (p < endOfTag) {
            char c = s[p];
            if (collectingValue && c != '"') {
                propValue += c; p++; continue;
            }
            if (collectingValue && c == '"') {
                collectingValue = false;
                propValues.push_back(propValue);
                propValue.clear();
                p++; continue;
            }
            if (c == '/') { isSingleTag = true; break; }
            if (c == '=') {
                propNames.push_back(propName);
                propName.clear();
                p++; // skip '='
                if (p < endOfTag && s[p] == '"') p++; // skip opening '"'
                collectingValue = true;
                continue;
            }
            if (c != ' ') { propName += c; }
            p++;
        }

        if (isSingleTag) {
            auto singleNode = std::make_unique<XmlNode>(tagName);
            singleNode->propertyNames = propNames;
            singleNode->propertyValues = propValues;
            stack.back()->subnodes.push_back(std::move(singleNode));
            continue;
        }

        // Collect text content
        std::string contentValue;
        if (x + 1 < split.size()) {
            const auto& nextS = split[x + 1];
            if (nextS.substr(0, 2) == "![") {
                // CDATA content
                auto cdataEnd = nextS.find("]]>");
                if (cdataEnd != std::string::npos && nextS.size() > 8) {
                    contentValue = nextS.substr(8, cdataEnd - 8);
                }
            } else {
                // Regular text content (after '>')
                if (endOfTag + 1 < s.size()) {
                    contentValue = s.substr(endOfTag + 1);
                }
            }
        } else if (endOfTag + 1 < s.size()) {
            contentValue = s.substr(endOfTag + 1);
        }

        auto newNode = std::make_unique<XmlNode>(tagName, contentValue);
        newNode->propertyNames = propNames;
        newNode->propertyValues = propValues;
        auto* nodePtr = newNode.get();
        stack.back()->subnodes.push_back(std::move(newNode));
        stack.push_back(nodePtr);
    }

    return mainNode;
}

// --- GP6 → GP5 conversion ---

void Gp6Parser::gp6NodeToGP5File(XmlNode* root) {
    if (!root) return;

    // Create a GP5 file to populate
    auto gp5 = std::make_unique<Gp5Parser>(std::vector<uint8_t>{});
    gp5->versionTuple[0] = 5;
    gp5->versionTuple[1] = 10;

    // Find GPIF root element (the XML wrapper adds an unnamed root above it)
    auto* gpifNode = root->getSubnodeByName("GPIF");
    if (!gpifNode) gpifNode = root; // fallback if GPIF is the root itself

    // Find main nodes (search from GPIF node, not the wrapper root)
    // MUST use directOnly=true because names like "Bars", "Voices", "Beats", "Notes"
    // also appear as children of MasterBar/Bar/Voice/Beat elements with different meaning
    auto* nScore = gpifNode->getSubnodeByName("Score", true);
    auto* nMasterTrack = gpifNode->getSubnodeByName("MasterTrack", true);
    auto* nTracks = gpifNode->getSubnodeByName("Tracks", true);
    auto* nMasterBars = gpifNode->getSubnodeByName("MasterBars", true);
    auto* nBars = gpifNode->getSubnodeByName("Bars", true);
    auto* nVoices = gpifNode->getSubnodeByName("Voices", true);
    auto* nBeats = gpifNode->getSubnodeByName("Beats", true);
    auto* nNotes = gpifNode->getSubnodeByName("Notes", true);
    auto* nRhythms = gpifNode->getSubnodeByName("Rhythms", true);

    if (!nScore || !nMasterTrack || !nTracks || !nMasterBars) {
        throw std::runtime_error("GP6/7: missing required XML nodes");
    }

    // Read score info
    if (auto* n = nScore->getSubnodeByName("Title", true)) gp5->title = n->content;
    if (auto* n = nScore->getSubnodeByName("SubTitle", true)) gp5->subtitle = n->content;
    if (auto* n = nScore->getSubnodeByName("Artist", true)) gp5->interpret = n->content;
    if (auto* n = nScore->getSubnodeByName("Album", true)) gp5->album = n->content;
    if (auto* n = nScore->getSubnodeByName("Words", true)) gp5->words = n->content;
    if (auto* n = nScore->getSubnodeByName("Music", true)) gp5->music = n->content;
    if (auto* n = nScore->getSubnodeByName("Copyright", true)) gp5->copyright = n->content;
    if (auto* n = nScore->getSubnodeByName("Tabber", true)) gp5->tab_author = n->content;
    if (auto* n = nScore->getSubnodeByName("Instructions", true)) gp5->instructional = n->content;

    // Read tempos from automations
    std::vector<GP6Tempo> tempos;
    if (nMasterTrack) {
        auto* nAutomations = nMasterTrack->getSubnodeByName("Automations", true);
        if (nAutomations) {
            for (const auto& nAuto : nAutomations->subnodes) {
                auto* nType = nAuto->getSubnodeByName("Type", true);
                if (nType && nType->content == "Tempo") {
                    GP6Tempo t;
                    auto* nLinear = nAuto->getSubnodeByName("Linear", true);
                    if (nLinear) t.linear = (nLinear->content == "true");
                    auto* nBar = nAuto->getSubnodeByName("Bar", true);
                    if (nBar) t.bar = std::stoi(nBar->content);
                    auto* nPosition = nAuto->getSubnodeByName("Position", true);
                    if (nPosition) t.position = std::stof(nPosition->content);
                    auto* nVisible = nAuto->getSubnodeByName("Visible", true);
                    if (nVisible) t.visible = (nVisible->content == "true");
                    auto* nValue = nAuto->getSubnodeByName("Value", true);
                    if (nValue) {
                        std::istringstream iss(nValue->content);
                        float tempoVal;
                        int tempoType;
                        iss >> tempoVal >> tempoType;
                        t.tempo = static_cast<int>(tempoVal);
                        t.tempoType = tempoType;
                    }
                    tempos.push_back(t);
                }
            }
        }
    }

    if (!tempos.empty()) {
        gp5->tempo = tempos[0].tempo;
    }

    // Read rhythms, chords
    auto rhythms = readRhythms(nRhythms);
    auto chords = readChords(nTracks);

    // Transfer tracks
    gp5->tracks = transferTracks(nTracks, gp5.get());
    gp5->trackCount = static_cast<int>(gp5->tracks.size());

    // Transfer measure headers
    gp5->measureHeaders = transferMeasureHeaders(nMasterBars, gp5.get());
    gp5->measureCount = static_cast<int>(gp5->measureHeaders.size());

    // Transfer lyrics
    gp5->lyrics = transferLyrics(nTracks);

    // Transfer bars (measures + beats + notes)
    if (nBars && nVoices && nBeats && nNotes) {
        // Create empty measures for each track × measure header
        for (auto& track : gp5->tracks) {
            for (auto& header : gp5->measureHeaders) {
                auto measure = std::make_unique<GpMeasure>(track.get(), header.get());
                track->addMeasure(std::move(measure));
            }
        }

        // Now fill in each bar
        int barIdx = 0;
        for (auto& nMasterBar : nMasterBars->subnodes) {
            if (nMasterBar->name != "MasterBar") continue;

            auto* nBarIds = nMasterBar->getSubnodeByName("Bars", true);
            if (!nBarIds) { barIdx++; continue; }

            // Parse bar IDs (space-separated)
            std::vector<int> barIds;
            std::istringstream iss(nBarIds->content);
            int id;
            while (iss >> id) barIds.push_back(id);

            for (size_t trackIdx = 0; trackIdx < barIds.size() && trackIdx < gp5->tracks.size(); trackIdx++) {
                int barId = barIds[trackIdx];
                // Find the bar node by ID
                XmlNode* nBar = nullptr;
                for (const auto& b : nBars->subnodes) {
                    if (!b->propertyValues.empty() && std::stoi(b->propertyValues[0]) == barId) {
                        nBar = b.get();
                        break;
                    }
                }
                if (!nBar) continue;

                auto* measure = gp5->tracks[trackIdx]->measures[barIdx].get();
                auto* nClef = nBar->getSubnodeByName("Clef", true);
                if (nClef) {
                    if (nClef->content == "F4") measure->clef = MeasureClef::bass;
                    else if (nClef->content == "C3") measure->clef = MeasureClef::tenor;
                    else if (nClef->content == "C4") measure->clef = MeasureClef::alto;
                    else if (nClef->content == "Neutral") measure->clef = MeasureClef::neutral;
                }

                auto* nSimileMark = nBar->getSubnodeByName("SimileMark", true);
                if (nSimileMark) {
                    if (nSimileMark->content == "Simple") measure->simileMark = SimileMark::simple;
                    else if (nSimileMark->content == "FirstOfDouble") measure->simileMark = SimileMark::firstOfDouble;
                    else if (nSimileMark->content == "SecondOfDouble") measure->simileMark = SimileMark::secondOfDouble;
                }

                // Transfer voices
                auto* nVoiceIds = nBar->getSubnodeByName("Voices", true);
                if (nVoiceIds) {
                    std::vector<int> voiceIds;
                    std::istringstream viss(nVoiceIds->content);
                    int vid;
                    while (viss >> vid) voiceIds.push_back(vid);

                    for (size_t vi = 0; vi < voiceIds.size() && vi < measure->voices.size(); vi++) {
                        if (voiceIds[vi] == -1) continue;

                        // Find voice node
                        XmlNode* nVoice = nullptr;
                        for (const auto& v : nVoices->subnodes) {
                            if (!v->propertyValues.empty() && std::stoi(v->propertyValues[0]) == voiceIds[vi]) {
                                nVoice = v.get();
                                break;
                            }
                        }
                        if (nVoice) {
                            transferVoice(nVoice, measure->voices[vi].get(),
                                          nBeats, nNotes, nRhythms, measure,
                                          rhythms, chords, tempos, barIdx);
                        }
                    }
                }
            }
            barIdx++;
        }
    }

    // Compute measure starts
    int start = Duration::quarterTime;
    for (auto& header : gp5->measureHeaders) {
        header->start = start;
        start += header->length();
    }

    self = gp5.release();
}

// --- Transfer methods ---

std::vector<std::unique_ptr<MeasureHeader>> Gp6Parser::transferMeasureHeaders(
    XmlNode* nMasterBars, Gp5Parser* song) {
    std::vector<std::unique_ptr<MeasureHeader>> headers;
    int cnt = 0;
    for (const auto& nMasterBar : nMasterBars->subnodes) {
        if (nMasterBar->name != "MasterBar") continue;

        auto header = std::make_unique<MeasureHeader>();

        // Key
        auto* nKey = nMasterBar->getSubnodeByName("Key", true);
        if (nKey && nKey->subnodes.size() >= 2) {
            int accidentals = std::stoi(nKey->subnodes[0]->content);
            int mode = (nKey->subnodes[1]->content == "Major") ? 0 : 1;
            header->keySignature = static_cast<KeySignature>(accidentals * 10 +
                ((accidentals < 0) ? -mode : mode));
        }

        header->hasDoubleBar = (nMasterBar->getSubnodeByName("DoubleBar", true) != nullptr);
        header->direction = transferDirections(nMasterBar->getSubnodeByName("Directions", true));
        header->fromDirection = transferFromDirections(nMasterBar->getSubnodeByName("Directions", true));

        // Repeat
        auto* nRepeat = nMasterBar->getSubnodeByName("Repeat", true);
        if (nRepeat && !nRepeat->propertyValues.empty()) {
            header->isRepeatOpen = (nRepeat->propertyValues.size() > 0 && nRepeat->propertyValues[0] == "true");
            header->repeatClose = 0;
            if (nRepeat->propertyValues.size() > 1 && nRepeat->propertyValues[1] == "true" &&
                nRepeat->propertyValues.size() > 2) {
                header->repeatClose = std::stoi(nRepeat->propertyValues[2]);
            }
        }

        // Alternate endings
        auto* nAE = nMasterBar->getSubnodeByName("AlternateEndings", true);
        if (nAE) {
            std::istringstream iss(nAE->content);
            int ae;
            while (iss >> ae) header->repeatAlternatives.push_back(ae);
        }

        // Time signature
        header->timeSignature = TimeSignature();
        auto* nTime = nMasterBar->getSubnodeByName("Time", true);
        if (nTime) {
            auto slashPos = nTime->content.find('/');
            if (slashPos != std::string::npos) {
                header->timeSignature.numerator = std::stoi(nTime->content.substr(0, slashPos));
                header->timeSignature.denominator.value = std::stoi(nTime->content.substr(slashPos + 1));
            }
        }

        // Triplet feel
        header->tripletFeel = TripletFeel::none;
        auto* nTF = nMasterBar->getSubnodeByName("TripletFeel", true);
        if (nTF) {
            if (nTF->content == "Triplet8th") header->tripletFeel = TripletFeel::eigth;
            else if (nTF->content == "Triplet16th") header->tripletFeel = TripletFeel::sixteenth;
            else if (nTF->content == "Dotted8th") header->tripletFeel = TripletFeel::dotted8th;
            else if (nTF->content == "Dotted16th") header->tripletFeel = TripletFeel::dotted16th;
            else if (nTF->content == "Scottish8th") header->tripletFeel = TripletFeel::scottish8th;
            else if (nTF->content == "Scottish16th") header->tripletFeel = TripletFeel::scottish16th;
        }

        header->song = song;
        header->number = cnt++;
        headers.push_back(std::move(header));
    }
    return headers;
}

std::vector<std::unique_ptr<GpTrack>> Gp6Parser::transferTracks(XmlNode* nTracks, Gp5Parser* song) {
    std::vector<std::unique_ptr<GpTrack>> result;
    int cnt = 0;
    for (const auto& nTrack : nTracks->subnodes) {
        auto track = std::make_unique<GpTrack>(song, cnt++);

        auto* nName = nTrack->getSubnodeByName("Name", true);
        if (nName) track->name = nName->content;

        auto* nColor = nTrack->getSubnodeByName("Color", true);
        if (nColor) {
            std::istringstream iss(nColor->content);
            int r, g, b;
            iss >> r >> g >> b;
            track->color = GpColor(r, g, b);
        }

        track->channel = GpMidiChannel();

        // RSE channel strip for volume/balance
        auto* nRSE = nTrack->getSubnodeByName("RSE", true);
        if (nRSE) {
            auto* nCS = nRSE->getSubnodeByName("ChannelStrip", true);
            if (nCS) {
                auto* nParams = nCS->getSubnodeByName("Parameters", true);
                if (nParams) {
                    std::istringstream iss(nParams->content);
                    std::vector<float> params;
                    float p;
                    while (iss >> p) params.push_back(p);
                    if (params.size() > 12) {
                        track->channel.balance = static_cast<int>(100 * params[11]);
                        track->channel.volume = static_cast<int>(100 * params[12]);
                    }
                }
            }
        }

        // MIDI info (GP6 vs GP7)
        auto* nMidi = nTrack->getSubnodeByName("GeneralMidi", true);
        if (nMidi) {
            // GP6 format
            auto* nProg = nMidi->getSubnodeByName("Program", true);
            if (nProg) track->channel.instrument = std::stoi(nProg->content);
            auto* nPri = nMidi->getSubnodeByName("PrimaryChannel", true);
            if (nPri) track->channel.channel = std::stoi(nPri->content);
            auto* nSec = nMidi->getSubnodeByName("SecondaryChannel", true);
            if (nSec) track->channel.effectChannel = std::stoi(nSec->content);
            auto* nPort = nMidi->getSubnodeByName("Port", true);
            if (nPort) track->port = std::stoi(nPort->content);
        } else {
            // GP7 format
            auto* nSounds = nTrack->getSubnodeByName("Sounds", true);
            if (nSounds && !nSounds->subnodes.empty()) {
                auto* nMidiNode = nSounds->subnodes[0]->getSubnodeByName("MIDI", true);
                if (nMidiNode) {
                    auto* nProg = nMidiNode->getSubnodeByName("Program", true);
                    if (nProg) track->channel.instrument = std::stoi(nProg->content);
                }
            }
            auto* nMidiConn = nTrack->getSubnodeByName("MidiConnection", true);
            if (nMidiConn) {
                auto* nPri = nMidiConn->getSubnodeByName("PrimaryChannel", true);
                if (nPri && !nPri->content.empty()) track->channel.channel = std::stoi(nPri->content);
                auto* nSec = nMidiConn->getSubnodeByName("SecondaryChannel", true);
                if (nSec && !nSec->content.empty()) track->channel.effectChannel = std::stoi(nSec->content);
                auto* nPort = nMidiConn->getSubnodeByName("Port", true);
                if (nPort && !nPort->content.empty()) track->port = std::stoi(nPort->content);
            }
        }

        // Tuning
        auto* nProps = nTrack->getSubnodeByName("Properties", true);
        if (nProps) {
            auto* nTuning = nProps->getSubnodeByProperty("name", "Tuning");
            if (nTuning && !nTuning->subnodes.empty()) {
                std::istringstream iss(nTuning->subnodes[0]->content);
                int gcnt = 0, val;
                while (iss >> val) {
                    track->strings.push_back(GuitarString(gcnt++, val));
                }
            }
            auto* nCapo = nProps->getSubnodeByProperty("name", "CapoFret");
            if (nCapo && !nCapo->subnodes.empty()) {
                track->offset = std::stoi(nCapo->subnodes[0]->content);
            }
            auto* nFrets = nProps->getSubnodeByProperty("name", "FretCount");
            if (nFrets && !nFrets->subnodes.empty()) {
                track->fretCount = std::stoi(nFrets->subnodes[0]->content);
            }
        }

        track->isPercussionTrack = (track->channel.channel == 9);

        auto* nPlayback = nTrack->getSubnodeByName("PlaybackState", true);
        if (nPlayback) {
            if (nPlayback->content == "Solo") track->isSolo = true;
            if (nPlayback->content == "Mute") track->isMute = true;
        }

        result.push_back(std::move(track));
    }
    return result;
}

std::vector<std::string> Gp6Parser::transferDirections(XmlNode* nDirections) {
    std::vector<std::string> result;
    if (!nDirections) return result;
    for (const auto& n : nDirections->subnodes) {
        if (n->name == "Target") result.push_back(n->content);
    }
    return result;
}

std::vector<std::string> Gp6Parser::transferFromDirections(XmlNode* nDirections) {
    std::vector<std::string> result;
    if (!nDirections) return result;
    for (const auto& n : nDirections->subnodes) {
        if (n->name == "Jump") result.push_back(n->content);
    }
    return result;
}

std::vector<Lyrics> Gp6Parser::transferLyrics(XmlNode* nTracks) {
    std::vector<Lyrics> result;
    if (!nTracks) return result;
    for (const auto& nTrack : nTracks->subnodes) {
        auto* nLyrics = nTrack->getSubnodeByName("Lyrics", true);
        Lyrics lyr;
        if (nLyrics) {
            int cnt = 0;
            for (const auto& nLine : nLyrics->subnodes) {
                if (cnt >= 5) break;
                if (nLine->subnodes.size() >= 2) {
                    lyr.lines[cnt].lyrics = nLine->subnodes[0]->content;
                    lyr.lines[cnt].startingMeasure = std::stoi(nLine->subnodes[1]->content);
                }
                cnt++;
            }
        }
        result.push_back(lyr);
    }
    return result;
}

void Gp6Parser::transferVoice(XmlNode* nVoice, GpVoice* voice,
                               XmlNode* nBeats, XmlNode* nNotes, XmlNode* nRhythms,
                               GpMeasure* measure, const std::vector<GP6Rhythm>& rhythms,
                               const std::vector<GP6Chord>& chords,
                               const std::vector<GP6Tempo>& tempos,
                               int masterBarIdx) {
    if (!nVoice) return;

    auto* nBeatIds = nVoice->getSubnodeByName("Beats", true);
    if (!nBeatIds) return;

    std::vector<int> beatIds;
    std::istringstream iss(nBeatIds->content);
    int bid;
    while (iss >> bid) beatIds.push_back(bid);

    int beatPos = measure->start();
    for (int beatId : beatIds) {
        // Find beat node
        XmlNode* nBeat = nullptr;
        for (const auto& b : nBeats->subnodes) {
            if (!b->propertyValues.empty() && std::stoi(b->propertyValues[0]) == beatId) {
                nBeat = b.get();
                break;
            }
        }
        if (!nBeat) continue;

        auto beat = std::make_unique<GpBeat>();
        beat->voice = voice;
        beat->start = beatPos;
        transferBeat(nBeat, beat.get(), nNotes, nRhythms, measure, rhythms, chords, tempos, masterBarIdx, beatPos);
        beatPos += beat->duration.time();
        voice->beats.push_back(std::move(beat));
    }
}

void Gp6Parser::transferBeat(XmlNode* nBeat, GpBeat* beat,
                              XmlNode* nNotes, XmlNode* nRhythms,
                              GpMeasure* measure, const std::vector<GP6Rhythm>& rhythms,
                              const std::vector<GP6Chord>& chords,
                              const std::vector<GP6Tempo>& tempos,
                              int masterBarIdx, int beatPos) {
    // Rhythm
    auto* nRhythmRef = nBeat->getSubnodeByName("Rhythm", true);
    if (nRhythmRef && !nRhythmRef->propertyValues.empty()) {
        int rhythmId = std::stoi(nRhythmRef->propertyValues[0]);
        if (rhythmId >= 0 && rhythmId < static_cast<int>(rhythms.size())) {
            const auto& rhythm = rhythms[rhythmId];
            beat->duration.value = rhythm.noteValue;
            beat->duration.isDotted = (rhythm.augmentationDots == 1);
            beat->duration.isDoubleDotted = (rhythm.augmentationDots == 2);
            beat->duration.tuplet = rhythm.primaryTuplet;
        }
    }

    // Dynamic (velocity)
    int velocity = Velocities::forte;
    auto* nDynamic = nBeat->getSubnodeByName("Dynamic", true);
    if (nDynamic) {
        if (nDynamic->content == "PPP") velocity = Velocities::pianoPianissimo;
        else if (nDynamic->content == "PP") velocity = Velocities::pianissimo;
        else if (nDynamic->content == "P") velocity = Velocities::piano;
        else if (nDynamic->content == "MP") velocity = Velocities::mezzoPiano;
        else if (nDynamic->content == "MF") velocity = Velocities::mezzoForte;
        else if (nDynamic->content == "F") velocity = Velocities::forte;
        else if (nDynamic->content == "FF") velocity = Velocities::fortissimo;
        else if (nDynamic->content == "FFF") velocity = Velocities::forteFortissimo;
    }

    // Tempo (mix table change)
    for (auto& t : const_cast<std::vector<GP6Tempo>&>(tempos)) {
        if (!t.transferred && t.bar == masterBarIdx && t.position == 0.0f) {
            beat->effect.mixTableChange = std::make_unique<MixTableChange>();
            beat->effect.mixTableChange->tempo = std::make_unique<MixTableItem>(t.tempo);
            t.transferred = true;
        }
    }

    // Brush/stroke
    auto* nProperties = nBeat->getSubnodeByName("Properties", true);
    if (nProperties) {
        auto* nBrush = nProperties->getSubnodeByProperty("name", "Brush");
        if (nBrush && !nBrush->subnodes.empty()) {
            std::string brushVal = nBrush->subnodes[0]->content;
            beat->effect.stroke = std::make_unique<BeatStroke>();
            if (brushVal == "Up") beat->effect.stroke->direction = BeatStrokeDirection::up;
            else beat->effect.stroke->direction = BeatStrokeDirection::down;
            auto* nBrushDur = nProperties->getSubnodeByProperty("name", "BrushDuration");
            if (nBrushDur && !nBrushDur->subnodes.empty()) {
                int dur = std::stoi(nBrushDur->subnodes[0]->content);
                beat->effect.stroke->setByGP6Standard(dur);
            }
        }
    }

    // Tremolo/wah
    auto* nTremolo = nBeat->getSubnodeByName("Tremolo", true);
    std::string tremoloStr;
    if (nTremolo) tremoloStr = nTremolo->content;

    // Grace note
    std::unique_ptr<GraceEffect> graceEffect;
    auto* nGraceNotes = nBeat->getSubnodeByName("GraceNotes", true);
    if (nGraceNotes) {
        graceEffect = std::make_unique<GraceEffect>();
        if (nGraceNotes->content == "BeforeBeat") graceEffect->isOnBeat = false;
        else graceEffect->isOnBeat = true;
        auto* nGrace = nBeat->getSubnodeByName("Grace", true);
        if (nGrace) {
            auto* nFret = nGrace->getSubnodeByName("Fret", true);
            if (nFret) graceEffect->fret = std::stoi(nFret->content);
        }
    }

    // Beat text
    auto* nFreeText = nBeat->getSubnodeByName("FreeText", true);
    if (nFreeText) {
        beat->text = std::make_unique<BeatText>(nFreeText->content);
    }

    // Fade
    auto* nFade = nBeat->getSubnodeByName("Fadding", true);
    if (nFade) {
        if (nFade->content == "FadeIn") beat->effect.fadeIn = true;
        else if (nFade->content == "FadeOut") beat->effect.fadeOut = true;
    }

    // Chord diagram
    auto* nChordId = nBeat->getSubnodeByName("Chord", true);
    if (nChordId && !chords.empty()) {
        // Could look up chord by ID
    }

    // Notes
    auto* nNoteIds = nBeat->getSubnodeByName("Notes", true);
    if (nNoteIds) {
        std::vector<int> noteIds;
        std::istringstream iss(nNoteIds->content);
        int nid;
        while (iss >> nid) noteIds.push_back(nid);

        beat->status = noteIds.empty() ? BeatStatus::rest : BeatStatus::normal;

        for (int noteId : noteIds) {
            XmlNode* nNote = nullptr;
            for (const auto& n : nNotes->subnodes) {
                if (!n->propertyValues.empty() && std::stoi(n->propertyValues[0]) == noteId) {
                    nNote = n.get();
                    break;
                }
            }
            if (!nNote) continue;

            auto note = std::make_unique<GpNote>(beat);
            transferNote(nNote, note.get(), beat, tremoloStr,
                         graceEffect.get(), velocity);
            beat->notes.push_back(std::move(note));
        }
    } else {
        beat->status = BeatStatus::rest;
    }
}

void Gp6Parser::transferNote(XmlNode* nNote, GpNote* note, GpBeat* beat,
                              const std::string& tremolo, GraceEffect* graceEffect,
                              int velocity) {
    auto* nProps = nNote->getSubnodeByName("Properties", true);
    if (nProps) {
        auto* nFret = nProps->getSubnodeByProperty("name", "Fret");
        if (nFret && !nFret->subnodes.empty()) {
            note->value = std::stoi(nFret->subnodes[0]->content);
        }
        auto* nString = nProps->getSubnodeByProperty("name", "String");
        if (nString && !nString->subnodes.empty()) {
            note->str = std::stoi(nString->subnodes[0]->content) + 1;
        }
        auto* nMidi = nProps->getSubnodeByProperty("name", "Midi");
        if (nMidi && !nMidi->subnodes.empty()) {
            note->midiNote = std::stoi(nMidi->subnodes[0]->content);
        }

        // Element + Variation for drums
        auto* nElem = nProps->getSubnodeByProperty("name", "Element");
        auto* nVar = nProps->getSubnodeByProperty("name", "Variation");
        if (nElem && nVar && !nElem->subnodes.empty() && !nVar->subnodes.empty()) {
            int elem = std::stoi(nElem->subnodes[0]->content);
            int var = std::stoi(nVar->subnodes[0]->content);
            note->midiNote = getGP6DrumValue(elem, var);
        }

        // Harmonics
        auto* nHType = nProps->getSubnodeByProperty("name", "HarmonicType");
        if (nHType && !nHType->subnodes.empty()) {
            std::string htype = nHType->subnodes[0]->content;
            if (htype == "Natural") note->effect.harmonic = std::make_unique<NaturalHarmonic>();
            else if (htype == "Artificial") note->effect.harmonic = std::make_unique<ArtificialHarmonic>();
            else if (htype == "Pinch") note->effect.harmonic = std::make_unique<PinchHarmonic>();
            else if (htype == "Tap") note->effect.harmonic = std::make_unique<TappedHarmonic>();
            else if (htype == "Semi") note->effect.harmonic = std::make_unique<SemiHarmonic>();
            else if (htype == "Feedback") note->effect.harmonic = std::make_unique<FeedbackHarmonic>();

            auto* nHFret = nProps->getSubnodeByProperty("name", "HarmonicFret");
            if (nHFret && !nHFret->subnodes.empty() && note->effect.harmonic) {
                note->effect.harmonic->fret = std::stof(nHFret->subnodes[0]->content);
            }
        }

        // Bends
        auto* nBend = nProps->getSubnodeByProperty("name", "Bended");
        if (nBend) {
            note->effect.bend = std::make_unique<BendEffect>();
            auto* nBendOrigin = nProps->getSubnodeByProperty("name", "BendOriginValue");
            auto* nBendMiddle = nProps->getSubnodeByProperty("name", "BendMiddleValue");
            auto* nBendDest = nProps->getSubnodeByProperty("name", "BendDestinationValue");
            auto* nBendOriginOff = nProps->getSubnodeByProperty("name", "BendOriginOffset");
            auto* nBendMiddleOff1 = nProps->getSubnodeByProperty("name", "BendMiddleOffset1");
            auto* nBendMiddleOff2 = nProps->getSubnodeByProperty("name", "BendMiddleOffset2");
            auto* nBendDestOff = nProps->getSubnodeByProperty("name", "BendDestinationOffset");

            float originVal = 0, middleVal = 0, destVal = 0;
            float originOff = 0, middleOff1 = 0, middleOff2 = 0, destOff = 100;

            if (nBendOrigin && !nBendOrigin->subnodes.empty())
                originVal = std::stof(nBendOrigin->subnodes[0]->content) / 50.0f * 2;
            if (nBendMiddle && !nBendMiddle->subnodes.empty())
                middleVal = std::stof(nBendMiddle->subnodes[0]->content) / 50.0f * 2;
            if (nBendDest && !nBendDest->subnodes.empty())
                destVal = std::stof(nBendDest->subnodes[0]->content) / 50.0f * 2;
            if (nBendOriginOff && !nBendOriginOff->subnodes.empty())
                originOff = std::stof(nBendOriginOff->subnodes[0]->content);
            if (nBendMiddleOff1 && !nBendMiddleOff1->subnodes.empty())
                middleOff1 = std::stof(nBendMiddleOff1->subnodes[0]->content);
            if (nBendMiddleOff2 && !nBendMiddleOff2->subnodes.empty())
                middleOff2 = std::stof(nBendMiddleOff2->subnodes[0]->content);
            if (nBendDestOff && !nBendDestOff->subnodes.empty())
                destOff = std::stof(nBendDestOff->subnodes[0]->content);

            note->effect.bend->points.push_back(BendPoint(originOff / 100.0f * 12, originVal));
            note->effect.bend->points.push_back(BendPoint(middleOff1 / 100.0f * 12, middleVal));
            note->effect.bend->points.push_back(BendPoint(middleOff2 / 100.0f * 12, middleVal));
            note->effect.bend->points.push_back(BendPoint(destOff / 100.0f * 12, destVal));
        }

        // Slides
        auto* nSlide = nProps->getSubnodeByProperty("name", "Slide");
        if (nSlide && !nSlide->subnodes.empty()) {
            int slideFlags = std::stoi(nSlide->subnodes[0]->content);
            if (slideFlags & 1) note->effect.slides.push_back(SlideType::shiftSlideTo);
            if (slideFlags & 2) note->effect.slides.push_back(SlideType::legatoSlideTo);
            if (slideFlags & 4) note->effect.slides.push_back(SlideType::outDownwards);
            if (slideFlags & 8) note->effect.slides.push_back(SlideType::outUpwards);
            if (slideFlags & 16) note->effect.slides.push_back(SlideType::intoFromBelow);
            if (slideFlags & 32) note->effect.slides.push_back(SlideType::intoFromAbove);
            if (slideFlags & 64) note->effect.slides.push_back(SlideType::pickScrapeOutDownwards);
            if (slideFlags & 128) note->effect.slides.push_back(SlideType::pickScrapeOutUpwards);
        }
    }

    // Left/right hand effects
    auto* nLetRing = nNote->getSubnodeByName("LetRing", true);
    note->effect.letRing = (nLetRing != nullptr);

    auto* nHammer = nNote->getSubnodeByName("HammerOn", true);
    note->effect.hammer = (nHammer != nullptr);

    auto* nVibrato = nNote->getSubnodeByName("Vibrato", true);
    note->effect.vibrato = (nVibrato != nullptr);

    auto* nPalmMute = nNote->getSubnodeByName("PalmMute", true);
    note->effect.palmMute = (nPalmMute != nullptr);

    // Accent
    auto* nAccent = nNote->getSubnodeByName("Accent", true);
    if (nAccent) {
        int val = std::stoi(nAccent->content);
        note->effect.accentuatedNote = (val == 4);
        note->effect.heavyAccentuatedNote = (val == 8);
        note->effect.staccato = (val == 1);
    }

    // Tie
    auto* nTie = nNote->getSubnodeByName("Tie", true);
    if (nTie && nTie->propertyValues.size() > 1 && nTie->propertyValues[1] == "true") {
        note->type = NoteType::tie;
    } else {
        note->type = NoteType::normal;
    }

    // Tremolo picking
    if (!tremolo.empty()) {
        note->effect.tremoloPicking = std::make_unique<TremoloPickingEffect>();
        note->effect.tremoloPicking->duration = Duration();
        if (tremolo == "1/2") note->effect.tremoloPicking->duration.value = 8;
        else if (tremolo == "1/4") note->effect.tremoloPicking->duration.value = 16;
        else if (tremolo == "1/8") note->effect.tremoloPicking->duration.value = 32;
    }

    if (graceEffect) {
        note->effect.grace = std::make_unique<GraceEffect>(*graceEffect);
    }

    note->velocity = velocity;
}

std::vector<GP6Rhythm> Gp6Parser::readRhythms(XmlNode* nRhythms) {
    std::vector<GP6Rhythm> result;
    if (!nRhythms) return result;
    int cnt = 0;
    static const std::string durations[] = {"Whole", "Half", "Quarter", "Eighth", "16th", "32nd"};
    for (const auto& nRhythm : nRhythms->subnodes) {
        auto* nNV = nRhythm->getSubnodeByName("NoteValue", true);
        int noteVal = 4;
        if (nNV) {
            for (int i = 0; i < 6; i++) {
                if (nNV->content == durations[i]) { noteVal = 1 << i; break; }
            }
        }
        int augCnt = 0;
        auto* nAug = nRhythm->getSubnodeByName("AugmentationDot", true);
        if (nAug && !nAug->propertyValues.empty()) {
            augCnt = std::stoi(nAug->propertyValues[0]);
        }
        int n = 1, m = 1;
        auto* nTuplet = nRhythm->getSubnodeByName("PrimaryTuplet", true);
        if (nTuplet && nTuplet->propertyValues.size() >= 2) {
            n = std::stoi(nTuplet->propertyValues[0]);
            m = std::stoi(nTuplet->propertyValues[1]);
        }
        result.push_back(GP6Rhythm(cnt++, noteVal, augCnt, n, m));
    }
    return result;
}

std::vector<GP6Chord> Gp6Parser::readChords(XmlNode* nTracks) {
    std::vector<GP6Chord> result;
    if (!nTracks) return result;
    int tcnt = 0;
    for (const auto& nTrack : nTracks->subnodes) {
        auto* nProps = nTrack->getSubnodeByName("Properties", true);
        if (nProps) {
            auto* nDiagrams = nProps->getSubnodeByProperty("name", "DiagramCollection");
            if (nDiagrams) {
                auto* nItems = nDiagrams->getSubnodeByName("Items", true);
                if (nItems) {
                    int chordcnt = 0;
                    for (const auto& item : nItems->subnodes) {
                        GP6Chord chord;
                        chord.id = chordcnt++;
                        chord.forTrack = tcnt;
                        if (item->propertyValues.size() > 1) chord.name = item->propertyValues[1];
                        result.push_back(chord);
                    }
                }
            }
        }
        tcnt++;
    }
    return result;
}

int Gp6Parser::getGP6DrumValue(int element, int variation) {
    int val = element * 10 + variation;
    switch (val) {
        case 0: return 35; case 10: return 38; case 11: return 91; case 12: return 37;
        case 20: return 99; case 30: return 56; case 40: return 102; case 50: return 43;
        case 60: return 45; case 70: return 47; case 80: return 48; case 90: return 50;
        case 100: return 42; case 101: return 92; case 102: return 46; case 110: return 44;
        case 120: return 57; case 130: return 49; case 140: return 55; case 150: return 51;
        case 151: return 93; case 152: return 53; case 160: return 52;
        default: return 0;
    }
}

// ============================================================
// GP7 Parser — ported from GP7File.cs
// ============================================================

Gp7Parser::Gp7Parser(const std::string& xml) : Gp6Parser(xml) {}

void Gp7Parser::readSong() {
    // GP7 reuses GP6 XML parsing
    auto root = parseGP6(xmlContent_);
    gp6NodeToGP5File(root.get());
}
