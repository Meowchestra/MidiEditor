#include "GpImporter.h"
#include "GpModels.h"
#include "GpBinaryReader.h"
#include "Gp12Parser.h"
#include "Gp345Parser.h"
#include "Gp678Parser.h"
#include "GpUnzip.h"
#include "GpToNative.h"
#include "GpMidiExport.h"

#include "../../midi/MidiFile.h"

#include <QTemporaryFile>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>

#include <memory>
#include <algorithm>
#include <cstring>

MidiFile* GpImporter::loadFile(QString path, bool* ok) {
    if (ok) *ok = false;

    try {
        std::string filePath = path.toStdString();
        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        // Read entire file
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "GpImporter: cannot open file:" << path;
            return nullptr;
        }
        QByteArray rawData = file.readAll();
        file.close();

        std::vector<uint8_t> data(rawData.begin(), rawData.end());

        std::unique_ptr<GpFile> gpFile;

        if (ext == "gp3") {
            gpFile = std::make_unique<Gp3Parser>(data);
            gpFile->readSong();
        } else if (ext == "gp4") {
            gpFile = std::make_unique<Gp4Parser>(data);
            gpFile->readSong();
        } else if (ext == "gp5") {
            gpFile = std::make_unique<Gp5Parser>(data);
            gpFile->readSong();
        } else if (ext == "gpx") {
            auto gp6 = std::make_unique<Gp6Parser>(data);
            gp6->readSong();
            gpFile = std::move(gp6);
            // Use the transferred GP5 file if available
            if (gpFile->self) {
                GpFile* transferred = gpFile->self;
                gpFile->self = nullptr;
                gpFile.reset(transferred);
            }
        } else if (ext == "gp" || ext == "gp7" || ext == "gp8") {
            // Could be ZIP (GP7/GP8) or legacy format
            if (data.size() >= 2 && data[0] == 0x50 && data[1] == 0x4B) {
                // ZIP archive — GP7/GP8
                GpUnzip unzip(data);
                std::vector<std::string> possiblePaths = {
                    "Content/score.gpif", "score.gpif", "content/score.gpif"
                };

                std::vector<uint8_t> xmlData;
                bool found = false;
                for (const auto& p : possiblePaths) {
                    try {
                        if (unzip.hasEntry(p)) {
                            xmlData = unzip.extract(p);
                            if (!xmlData.empty()) { found = true; break; }
                        }
                    } catch (...) {
                        // Try next path
                    }
                }

                if (!found) {
                    qWarning() << "GpImporter: could not find score.gpif in archive";
                    return nullptr;
                }

                std::string xml(xmlData.begin(), xmlData.end());
                auto gp7 = std::make_unique<Gp7Parser>(xml);
                gp7->readSong();
                gpFile = std::move(gp7);

                if (gpFile->self) {
                    GpFile* transferred = gpFile->self;
                    gpFile->self = nullptr;
                    gpFile.reset(transferred);
                }
            } else {
                // Legacy GP file — detect version from header
                std::string header(data.begin(),
                                   data.begin() + std::min(data.size(), static_cast<size_t>(50)));

                if (header.find("v5.") != std::string::npos ||
                    header.find("v 5.") != std::string::npos) {
                    gpFile = std::make_unique<Gp5Parser>(data);
                } else if (header.find("v4.") != std::string::npos ||
                           header.find("v 4.") != std::string::npos) {
                    gpFile = std::make_unique<Gp4Parser>(data);
                } else if (header.find("v3.") != std::string::npos ||
                           header.find("v 3.") != std::string::npos) {
                    gpFile = std::make_unique<Gp3Parser>(data);
                } else {
                    // Default to GP5
                    qDebug() << "GpImporter: unknown version, trying GP5 parser";
                    gpFile = std::make_unique<Gp5Parser>(data);
                }
                gpFile->readSong();
            }
        } else if (ext == "gtp") {
            // GP1/GP2 legacy format — detect version from header
            std::string header(data.begin(),
                               data.begin() + std::min(data.size(), static_cast<size_t>(50)));

            if (header.find("GUITARE") != std::string::npos) {
                // French "GUITARE" → GP1 (v1.xx)
                gpFile = std::make_unique<Gp1Parser>(data);
            } else {
                // English "GUITAR PRO v2" → GP2 (v2.xx)
                gpFile = std::make_unique<Gp2Parser>(data);
            }
            gpFile->readSong();
        } else {
            qWarning() << "GpImporter: unknown file extension:" << ext;
            return nullptr;
        }

        // Convert to MIDI
        GpFile* effectiveFile = gpFile->effective();
        NativeFormat format(effectiveFile);
        GpMidiExport midiExport = format.toMidi();
        std::vector<uint8_t> midiBytes = midiExport.createBytes();

        if (midiBytes.empty()) {
            qWarning() << "GpImporter: failed to generate MIDI data";
            return nullptr;
        }

        // Write to temp file
        QTemporaryFile tempFile;
        tempFile.setFileTemplate(QDir::tempPath() + "/gpimport_XXXXXX.mid");
        tempFile.setAutoRemove(false); // We'll remove after loading
        if (!tempFile.open()) {
            qWarning() << "GpImporter: cannot create temp file";
            return nullptr;
        }

        tempFile.write(reinterpret_cast<const char*>(midiBytes.data()),
                       static_cast<qint64>(midiBytes.size()));
        QString tempPath = tempFile.fileName();
        tempFile.close();

        // Load via MidiFile
        bool midiOk = false;
        MidiFile* midiFile = new MidiFile(tempPath, &midiOk);

        // Clean up temp file
        QFile::remove(tempPath);

        if (!midiOk || !midiFile) {
            qWarning() << "GpImporter: failed to load generated MIDI file";
            delete midiFile;
            return nullptr;
        }

        // Set the original path so the file is associated with the GP file
        midiFile->setPath(path);

        if (ok) *ok = true;
        return midiFile;

    } catch (const std::exception& e) {
        qWarning() << "GpImporter: exception:" << e.what();
        return nullptr;
    } catch (...) {
        qWarning() << "GpImporter: unknown exception";
        return nullptr;
    }
}
