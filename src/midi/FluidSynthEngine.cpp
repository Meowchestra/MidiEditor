/*
 * MidiEditor
 * Copyright (C) 2010  Markus Schwenk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef FLUIDSYNTH_SUPPORT

#include "FluidSynthEngine.h"

#include <fluidsynth.h>

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMutexLocker>
#include <QSettings>
#include <QThreadPool>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

// ============================================================================
// Singleton
// ============================================================================

FluidSynthEngine* FluidSynthEngine::instance() {
    static FluidSynthEngine engine;
    return &engine;
}

FluidSynthEngine::FluidSynthEngine()
    : QObject(nullptr),
      _settings(nullptr),
      _synth(nullptr),
      _audioDriver(nullptr),
      _initialized(false),
      _audioDriverName("wasapi"),
      _sampleRate(48000.0),
      _sampleFormat("16bits"),
      _gain(0.5),
      _reverbEngine("fdn"),
      _reverbEnabled(true),
      _chorusEnabled(true),
      _polyphony(256),
      _isStackUpdatePending(false),
      _isEngineRestartPending(false) {
}

FluidSynthEngine::~FluidSynthEngine() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool FluidSynthEngine::initialize() {
    QMutexLocker locker(&_engineMutex);
    
    if (_initialized) {
        return true;
    }

    // Create FluidSynth settings
    _settings = new_fluid_settings();
    if (!_settings) {
        emit initializationFailed(tr("Failed to create FluidSynth settings"));
        return false;
    }

    // Apply audio settings
    if (!_audioDriverName.isEmpty()) {
        fluid_settings_setstr(_settings, "audio.driver", _audioDriverName.toUtf8().constData());
    }
    fluid_settings_setnum(_settings, "synth.gain", _gain);
    fluid_settings_setnum(_settings, "synth.sample-rate", _sampleRate);
    fluid_settings_setint(_settings, "synth.reverb.active", _reverbEnabled ? 1 : 0);
    fluid_settings_setint(_settings, "synth.chorus.active", _chorusEnabled ? 1 : 0);
    fluid_settings_setint(_settings, "synth.polyphony", _polyphony);
    // Set custom reverb engine only if supported (wrap string in literal or standard way to avoid errors if fallback is used)
    fluid_settings_setstr(_settings, "synth.reverb.engine", _reverbEngine.toUtf8().constData());

    // Audio Driver Settings
    fluid_settings_setstr(_settings, "audio.sample-format", _sampleFormat.toUtf8().constData());

    // Enable dynamic sample loading to prevent massive upfront decompresion lag for SF3
    fluid_settings_setint(_settings, "synth.dynamic-sample-loading", 1);

    // Create the synthesizer
    _synth = new_fluid_synth(_settings);
    if (!_synth) {
        delete_fluid_settings(_settings);
        _settings = nullptr;
        emit initializationFailed(tr("Failed to create FluidSynth synthesizer"));
        return false;
    }

    // Create the audio driver
    _audioDriver = new_fluid_audio_driver(_settings, _synth);
    if (!_audioDriver) {
        delete_fluid_synth(_synth);
        _synth = nullptr;
        delete_fluid_settings(_settings);
        _settings = nullptr;
        emit initializationFailed(tr("Failed to create FluidSynth audio driver"));
        return false;
    }

    _initialized = true;
    qDebug() << "FluidSynth engine initialized successfully";
    qDebug() << "  Audio Driver:"    << _audioDriverName;
    qDebug() << "  Sample Rate:"     << _sampleRate;
    qDebug() << "  Sample Format:"   << _sampleFormat;
    qDebug() << "  Gain:"            << _gain;
    qDebug() << "  Polyphony:"       << _polyphony;

    // Load the active SoundFont collection into the engine
    QStringList enabledPaths;
    for (const auto &pair : _collection) {
        if (pair.second) {
            enabledPaths.append(pair.first);
        }
    }
    
    if (!enabledPaths.isEmpty() && !_isStackUpdatePending) {
        _isStackUpdatePending = true;
        _pendingStackUpdate = enabledPaths;
        QThreadPool::globalInstance()->start([this]() {
            while (true) {
                QStringList currentPaths;
                {
                    QMutexLocker lock(&_engineMutex);
                    if (!_isStackUpdatePending) break;
                    currentPaths = _pendingStackUpdate;
                    _isStackUpdatePending = false;
                }
                
                unloadAllSoundFonts();
                for (int i = currentPaths.size() - 1; i >= 0; --i) {
                    loadSoundFont(currentPaths[i]);
                }
                
                QMetaObject::invokeMethod(this, "soundFontsChanged", Qt::QueuedConnection);
            }
        });
    }

    return true;
}

void FluidSynthEngine::shutdown() {
    QMutexLocker locker(&_engineMutex);
    
    if (!_initialized) {
        return;
    }

    if (_audioDriver) {
        delete_fluid_audio_driver(_audioDriver);
        _audioDriver = nullptr;
    }

    if (_synth) {
        // Unload all SoundFonts
        for (const auto &pair : _loadedFonts) {
            fluid_synth_sfunload(_synth, pair.first, 1);
        }
        _loadedFonts.clear();

        delete_fluid_synth(_synth);
        _synth = nullptr;
    }

    if (_settings) {
        delete_fluid_settings(_settings);
        _settings = nullptr;
    }

    _initialized = false;
    qDebug() << "FluidSynth engine shut down";
}

bool FluidSynthEngine::isInitialized() const {
    return _initialized;
}

// ============================================================================
// SoundFont Management
// ============================================================================

int FluidSynthEngine::loadSoundFont(const QString &path) {
    QMutexLocker locker(&_engineMutex);
    
    if (!_initialized || !_synth) {
        qWarning() << "FluidSynth: Cannot load SoundFont - engine not initialized";
        return -1;
    }

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isReadable()) {
        qWarning() << "FluidSynth: SoundFont file not found or not readable:" << path;
        return -1;
    }

    // Check for duplicates
    QString canonicalPath = fi.canonicalFilePath();
    for (const auto &pair : _loadedFonts) {
        if (QFileInfo(pair.second).canonicalFilePath() == canonicalPath) {
            qDebug() << "FluidSynth: SoundFont already loaded:" << path;
            return pair.first;
        }
    }

    // reset_presets=1 means FluidSynth recalculates preset assignments after loading
    int sfontId = fluid_synth_sfload(_synth, path.toUtf8().constData(), 1);
    if (sfontId == FLUID_FAILED) {
        qWarning() << "FluidSynth: Failed to load SoundFont:" << path;
        return -1;
    }

    _loadedFonts.append(qMakePair(sfontId, path));
    qDebug() << "FluidSynth: Loaded SoundFont" << path << "with id" << sfontId;
    emit soundFontLoaded(sfontId, path);
    return sfontId;
}

void FluidSynthEngine::setSoundFontCollection(const QList<QPair<QString, bool>> &collection) {
    QMutexLocker locker(&_engineMutex);
    _collection = collection;
    
    // Auto-update the stack for enabled items
    QStringList enabledPaths;
    for (const auto &pair : _collection) {
        if (pair.second) {
            enabledPaths.append(pair.first);
        }
    }
    setSoundFontStack(enabledPaths);
}

void FluidSynthEngine::addSoundFonts(const QStringList &paths) {
    QMutexLocker locker(&_engineMutex);
    bool changed = false;
    for (const QString &path : paths) {
        QString canonPath = QFileInfo(path).canonicalFilePath();
        bool exists = false;
        for (const auto &pair : _collection) {
            if (QFileInfo(pair.first).canonicalFilePath() == canonPath) {
                exists = true;
                break;
            }
        }
        if (!exists && !canonPath.isEmpty()) {
            _collection.append(qMakePair(canonPath, false));
            changed = true;
        }
    }
    if (changed) {
        emit soundFontsChanged();
    }
}

bool FluidSynthEngine::unloadSoundFont(int sfontId) {
    QMutexLocker locker(&_engineMutex);
    
    if (!_initialized || !_synth) {
        return false;
    }

    if (fluid_synth_sfunload(_synth, sfontId, 1) == FLUID_OK) {
        for (int i = 0; i < _loadedFonts.size(); ++i) {
            if (_loadedFonts[i].first == sfontId) {
                _loadedFonts.removeAt(i);
                break;
            }
        }
        emit soundFontUnloaded(sfontId);
        return true;
    }
    return false;
}

void FluidSynthEngine::unloadAllSoundFonts() {
    QMutexLocker locker(&_engineMutex);
    
    if (!_initialized || !_synth) {
        return;
    }

    // Unload in reverse order (highest priority first)
    for (int i = _loadedFonts.size() - 1; i >= 0; --i) {
        fluid_synth_sfunload(_synth, _loadedFonts[i].first, 1);
    }
    _loadedFonts.clear();
}

QList<QPair<int, QString>> FluidSynthEngine::loadedSoundFonts() const {
    QMutexLocker locker(&_engineMutex);
    if (_initialized) {
        return _loadedFonts;
    }
    // Engine not initialized — return synthetic entries from pending paths
    // so the UI can still display them
    QList<QPair<int, QString>> result;
    for (int i = 0; i < _pendingStackUpdate.size(); ++i) {
        // Use negative IDs as placeholders since there's no real sfont_id
        result.append(qMakePair(-(i + 1), _pendingStackUpdate[i]));
    }
    // Reverse so that first in pending (highest priority) = last in result (matching load order convention)
    std::reverse(result.begin(), result.end());
    return result;
}

void FluidSynthEngine::setSoundFontStack(const QStringList &paths) {
    QMutexLocker locker(&_engineMutex);
    if (!_initialized || !_synth) {
        return;
    }

    _pendingStackUpdate = paths;
    if (!_isStackUpdatePending) {
        _isStackUpdatePending = true;
        
        QThreadPool::globalInstance()->start([this]() {
            while (true) {
                // Wait briefly to allow rapid UI toggles/scrolls to buffer
                QThread::msleep(150);
                
                QStringList currentPaths;
                {
                    QMutexLocker lock(&_engineMutex);
                    if (!_isStackUpdatePending) break;
                    currentPaths = _pendingStackUpdate;
                    _isStackUpdatePending = false;
                }
                
                QMutexLocker lock(&_engineMutex);
                if (!_initialized || !_synth) break;
                
                // Convert top-down UI order to bottom-up engine stack order
                QStringList targetLoadOrder;
                for (int i = currentPaths.size() - 1; i >= 0; --i) {
                    targetLoadOrder.append(currentPaths[i]);
                }
                
                // Find untouched common prefix
                int commonPrefix = 0;
                while (commonPrefix < _loadedFonts.size() && commonPrefix < targetLoadOrder.size()) {
                    if (QFileInfo(_loadedFonts[commonPrefix].second).canonicalFilePath() == 
                        QFileInfo(targetLoadOrder[commonPrefix]).canonicalFilePath()) {
                        commonPrefix++;
                    } else {
                        break;
                    }
                }
                
                // Unload top layers that diverge
                for (int i = _loadedFonts.size() - 1; i >= commonPrefix; --i) {
                    fluid_synth_sfunload(_synth, _loadedFonts[i].first, 1);
                }
                // Cleanup removed layers from tracking
                while (_loadedFonts.size() > commonPrefix) {
                    _loadedFonts.removeLast();
                }
                
                // Load missing layers in the correct order
                for (int i = commonPrefix; i < targetLoadOrder.size(); ++i) {
                    loadSoundFont(targetLoadOrder[i]);
                }
                
                QMetaObject::invokeMethod(this, "soundFontsChanged", Qt::QueuedConnection);
            }
        });
    }
}

// ============================================================================
// MIDI Message Routing
// ============================================================================

void FluidSynthEngine::sendMidiData(const QByteArray &data) {
    QMutexLocker locker(&_engineMutex);
    if (!_initialized || !_synth || data.isEmpty()) {
        return;
    }

    unsigned char status = static_cast<unsigned char>(data[0]);
    unsigned char type = status & 0xF0;
    int channel = status & 0x0F;

    // Handle SysEx
    if (status == 0xF0) {
        fluid_synth_sysex(_synth,
                          data.constData() + 1,
                          data.size() - 1,
                          nullptr, nullptr, nullptr, 0);
        return;
    }

    // System real-time messages (single byte) — ignore
    if (status >= 0xF8) {
        return;
    }

    // Channel messages
    switch (type) {
    case 0x90: // Note On
        if (data.size() >= 3) {
            int key = static_cast<unsigned char>(data[1]);
            int vel = static_cast<unsigned char>(data[2]);
            if (vel == 0) {
                fluid_synth_noteoff(_synth, channel, key);
            } else {
                fluid_synth_noteon(_synth, channel, key, vel);
            }
        }
        break;

    case 0x80: // Note Off
        if (data.size() >= 3) {
            int key = static_cast<unsigned char>(data[1]);
            fluid_synth_noteoff(_synth, channel, key);
        }
        break;

    case 0xC0: // Program Change
        if (data.size() >= 2) {
            int program = static_cast<unsigned char>(data[1]);
            fluid_synth_program_change(_synth, channel, program);
        }
        break;

    case 0xB0: // Control Change
        if (data.size() >= 3) {
            int ctrl = static_cast<unsigned char>(data[1]);
            int value = static_cast<unsigned char>(data[2]);
            fluid_synth_cc(_synth, channel, ctrl, value);
        }
        break;

    case 0xE0: // Pitch Bend
        if (data.size() >= 3) {
            int lsb = static_cast<unsigned char>(data[1]);
            int msb = static_cast<unsigned char>(data[2]);
            int value = (msb << 7) | lsb;
            fluid_synth_pitch_bend(_synth, channel, value);
        }
        break;

    case 0xD0: // Channel Pressure (Aftertouch)
        if (data.size() >= 2) {
            int pressure = static_cast<unsigned char>(data[1]);
            fluid_synth_channel_pressure(_synth, channel, pressure);
        }
        break;

    case 0xA0: // Key Pressure (Polyphonic Aftertouch)
        if (data.size() >= 3) {
            int key = static_cast<unsigned char>(data[1]);
            int pressure = static_cast<unsigned char>(data[2]);
            fluid_synth_key_pressure(_synth, channel, key, pressure);
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// Export
// ============================================================================

bool FluidSynthEngine::needsTranscode(const AudioExportSettings &settings) {
    // All formats now go through transcoding to ensure maximum compatibility and precision.
    Q_UNUSED(settings);
    return true;
}

void FluidSynthEngine::exportAudio(const QString &midiFilePath, const QString &outputPath,
                                   const AudioExportSettings &settings, int totalTicks) {
    _exportCancelled.store(false);

    if (!_initialized) {
        emit exportFinished(false, outputPath);
        return;
    }

    // Transcode-All Model: render to a temporary high-precision WAV first, 
    // then transcode via libsndfile to the final destination.
    const bool transcode = true; 
    QString renderPath =
        QDir::tempPath() + "/MidiEditor_render_" +
        QString::number(QCoreApplication::applicationPid()) + ".wav";

    fluid_settings_t* expSettings = new_fluid_settings();
    fluid_settings_setstr(expSettings, "audio.driver", "file");
    fluid_settings_setstr(expSettings, "audio.file.name", renderPath.toUtf8().constData());

    QString fileType = "wav";
    double sampleRate = settings.sampleRate();
    QString fileFormat = "float"; // Intermediate render is always high-precision float

    // Opus strictly requires 48kHz.
    if (settings.format == AudioExportSettings::OPUS) {
        sampleRate = 48000.0;
    }

    fluid_settings_setstr(expSettings, "audio.file.type", fileType.toUtf8().constData());
    fluid_settings_setstr(expSettings, "audio.file.format", fileFormat.toUtf8().constData());
    fluid_settings_setstr(expSettings, "audio.sample-format", "float"); // Internal processing always float
    fluid_settings_setnum(expSettings, "synth.sample-rate", sampleRate);

    qDebug() << "FluidSynthEngine::exportAudio: Rendering at" << sampleRate << "Hz";
    qDebug() << "FluidSynthEngine::exportAudio: Starting export to" << outputPath << "Format:" << settings.format << "Transcode:" << transcode;
    qDebug() << "FluidSynthEngine::exportAudio: MIDI total ticks:" << totalTicks;
    // Copy synth settings from the live engine
    _engineMutex.lock();
    fluid_settings_setint(expSettings, "synth.reverb.active", _reverbEnabled ? 1 : 0);
    fluid_settings_setint(expSettings, "synth.chorus.active", _chorusEnabled ? 1 : 0);
    fluid_settings_setnum(expSettings, "synth.gain", _gain);
    fluid_settings_setint(expSettings, "synth.polyphony", _polyphony);

    fluid_settings_setstr(expSettings, "synth.reverb.engine", _reverbEngine.toUtf8().constData());

    // Get loaded font paths while holding lock
    QStringList fontsToLoad;
    for (int i = 0; i < _loadedFonts.size(); ++i) {
        fontsToLoad.append(_loadedFonts[i].second);
    }
    _engineMutex.unlock();

    fluid_synth_t* synth = new_fluid_synth(expSettings);
    if (!synth) {
        delete_fluid_settings(expSettings);
        emit exportFinished(false, outputPath);
        return;
    }

    for (const QString &f : fontsToLoad) {
        fluid_synth_sfload(synth, f.toUtf8().constData(), 1);
    }

    fluid_player_t* player = new_fluid_player(synth);
    if (fluid_player_add(player, midiFilePath.toUtf8().constData()) != FLUID_OK) {
        delete_fluid_player(player);
        delete_fluid_synth(synth);
        delete_fluid_settings(expSettings);
        emit exportFinished(false, outputPath);
        return;
    }

    fluid_player_play(player);
    fluid_file_renderer_t* renderer = new_fluid_file_renderer(synth);
    if (!renderer) {
        delete_fluid_player(player);
        delete_fluid_synth(synth);
        delete_fluid_settings(expSettings);
        emit exportFinished(false, outputPath);
        return;
    }

    // Use the provided total ticks for progress reporting
    if (totalTicks <= 0) totalTicks = 1;

    int lastPercent = -1;
    bool cancelled = false;

    // Render the MIDI content
    while (fluid_player_get_status(player) == FLUID_PLAYER_PLAYING) {
        if (_exportCancelled.load()) {
            cancelled = true;
            break;
        }
        if (fluid_file_renderer_process_block(renderer) != FLUID_OK) {
            break;
        }
        int curTick = fluid_player_get_current_tick(player);
        
        // Scale progress: MIDI rendering phase is 0-80% if transcoding, 0-100% if not.
        double midiScale = transcode ? 80.0 : 100.0;
        int stagePercent = transcode ? 80 : 100;
        int percent = static_cast<int>(midiScale * static_cast<double>(curTick) / totalTicks);
        
        // CAP the progress to the stage mark!
        if (percent > stagePercent) percent = stagePercent;

        if (percent != lastPercent) {
            lastPercent = percent;
            emit exportProgress(percent);
        }
    }

    // Render reverb tail: 2 seconds of silence so reverb/sustain fades naturally
    if (!cancelled && settings.includeReverbTail) {
        int tailFrames = static_cast<int>(settings.sampleRate() * 2.0);
        int framesPerBlock = 64; // FluidSynth default block size
        fluid_settings_getint(expSettings, "audio.period-size", &framesPerBlock);
        if (framesPerBlock <= 0) framesPerBlock = 64;
        int totalBlocks = tailFrames / framesPerBlock;
        if (totalBlocks <= 0) totalBlocks = 1;

        for (int i = 0; i < totalBlocks; ++i) {
            if (_exportCancelled.load()) {
                cancelled = true;
                break;
            }
            fluid_file_renderer_process_block(renderer);
        }
    }

    delete_fluid_file_renderer(renderer);
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(expSettings);

    if (cancelled) {
        QFile::remove(outputPath);
        emit exportCancelled();
        return;
    }

    // Stage 2: Transcode if needed
    if (transcode) {
        bool ok = transcodeWavTo(renderPath, outputPath, settings);
        QFile::remove(renderPath); // clean up temp file
        if (!ok) {
            emit exportFinished(false, outputPath);
            return;
        }
    } else {
        // For WAV/FLAC/OGG, FluidSynth rendered to the temp file, now move it
        if (QFile::exists(outputPath)) QFile::remove(outputPath);
        if (!QFile::copy(renderPath, outputPath)) {
            qWarning() << "FluidSynthEngine::exportAudio: failed to copy file to" << outputPath;
            QFile::remove(renderPath);
            emit exportFinished(false, outputPath);
            return;
        }
    }
    QFile::remove(renderPath); // clean up temp file
    emit exportFinished(true, outputPath);
}

void FluidSynthEngine::cancelExport() {
    _exportCancelled.store(true);
}

// ============================================================================
// Transcode helper — dynamically loads libsndfile to convert WAV → Opus/MP3
// ============================================================================

// Minimal libsndfile types needed for dynamic loading (avoids sndfile.h header dependency)
namespace sndfile_dyn {

// From sndfile.h — we replicate only what we need
typedef int64_t sf_count_t;

struct SF_INFO {
    sf_count_t frames;
    int        samplerate;
    int        channels;
    int        format;
    int        sections;
    int        seekable;
};

typedef void* SNDFILE_HANDLE;  // opaque

// Format constants from sndfile.h
constexpr int SF_FORMAT_WAV            = 0x010000;
constexpr int SF_FORMAT_FLAC           = 0x170000;
constexpr int SF_FORMAT_OGG            = 0x200000;
constexpr int SF_FORMAT_MPEG           = 0x230000;
constexpr int SF_FORMAT_PCM_16         = 0x0002;
constexpr int SF_FORMAT_PCM_24         = 0x0003;
constexpr int SF_FORMAT_PCM_32         = 0x0004;
constexpr int SF_FORMAT_FLOAT          = 0x0006;
constexpr int SF_FORMAT_VORBIS         = 0x0060;
constexpr int SF_FORMAT_OPUS           = 0x0064;
constexpr int SF_FORMAT_MPEG_LAYER_III = 0x0082;

// Open modes
constexpr int SFM_READ                 = 0x10;
constexpr int SFM_WRITE                = 0x20;

// Function pointer types
using fn_sf_open         = SNDFILE_HANDLE (*)(const char*, int, SF_INFO*);
using fn_sf_close        = int            (*)(SNDFILE_HANDLE);
using fn_sf_readf_float  = sf_count_t     (*)(SNDFILE_HANDLE, float*, sf_count_t);
using fn_sf_writef_float = sf_count_t     (*)(SNDFILE_HANDLE, const float*, sf_count_t);
using fn_sf_strerror     = const char*    (*)(SNDFILE_HANDLE);

#ifdef Q_OS_WIN
// Windows-only: sf_wchar_open accepts wchar_t* paths for proper Unicode support.
// libsndfile's sf_open() uses the C runtime fopen() which on MSVC expects ANSI
// (system code page), NOT UTF-8 — causing garbled filenames for non-ASCII text.
using fn_sf_wchar_open   = SNDFILE_HANDLE   (*)(const wchar_t*, int, SF_INFO*);
#endif

} // namespace sndfile_dyn

bool FluidSynthEngine::transcodeWavTo(const QString &wavPath, const QString &outputPath,
                                       const AudioExportSettings &settings) {
    // Load libsndfile dynamically — it ships alongside the app as sndfile.dll / libsndfile.so
    QLibrary sndLib;
    QStringList candidates = {"sndfile", "libsndfile", "libsndfile-1", "sndfile-1"};
    bool loaded = false;
    for (const QString &name : candidates) {
        sndLib.setFileName(name);
        if (sndLib.load()) { loaded = true; break; }
        // Also try from the app directory explicitly
        QString appDir = QCoreApplication::applicationDirPath();
        sndLib.setFileName(appDir + "/" + name);
        if (sndLib.load()) { loaded = true; break; }
    }
    if (!loaded) {
        qWarning() << "FluidSynthEngine::transcodeWavTo: could not load libsndfile:"
                    << sndLib.errorString();
        return false;
    }

    // Resolve function pointers
    auto sf_open         = reinterpret_cast<sndfile_dyn::fn_sf_open>(sndLib.resolve("sf_open"));
    auto sf_close        = reinterpret_cast<sndfile_dyn::fn_sf_close>(sndLib.resolve("sf_close"));
    auto sf_readf_float  = reinterpret_cast<sndfile_dyn::fn_sf_readf_float>(sndLib.resolve("sf_readf_float"));
    auto sf_writef_float = reinterpret_cast<sndfile_dyn::fn_sf_writef_float>(sndLib.resolve("sf_writef_float"));
    auto sf_strerror     = reinterpret_cast<sndfile_dyn::fn_sf_strerror>(sndLib.resolve("sf_strerror"));

#ifdef Q_OS_WIN
    auto sf_wchar_open   = reinterpret_cast<sndfile_dyn::fn_sf_wchar_open>(sndLib.resolve("sf_wchar_open"));
#endif

    if (!sf_open || !sf_close || !sf_readf_float || !sf_writef_float) {
        qWarning() << "FluidSynthEngine::transcodeWavTo: failed to resolve libsndfile symbols";
        return false;
    }

    // Helper lambda: open a file using the best available function for the platform.
    // On Windows, prefer sf_wchar_open for proper Unicode path support.
    auto openSndFile = [&](const QString &path, int mode, sndfile_dyn::SF_INFO *info) -> sndfile_dyn::SNDFILE_HANDLE {
#ifdef Q_OS_WIN
        if (sf_wchar_open) {
            std::wstring wpath = path.toStdWString();
            return sf_wchar_open(wpath.c_str(), mode, info);
        }
#endif
        return sf_open(path.toUtf8().constData(), mode, info);
    };

    // Open the source WAV
    sndfile_dyn::SF_INFO srcInfo = {};
    auto srcFile = openSndFile(wavPath, sndfile_dyn::SFM_READ, &srcInfo);
    if (!srcFile) {
        qWarning() << "FluidSynthEngine::transcodeWavTo: failed to open source WAV" << wavPath
                    << "Error:" << (sf_strerror ? sf_strerror(nullptr) : "unknown error");
        return false;
    }
    qDebug() << "FluidSynthEngine::transcodeWavTo: source WAV opened. Channels:" << srcInfo.channels << "Rate:" << srcInfo.samplerate << "Frames:" << srcInfo.frames;

    // Determine target format (source is always float WAV)
    int dstFormat = 0;
    bool isFloat = (settings.sampleFormat == "float");

    switch (settings.format) {
        case AudioExportSettings::WAV:
            dstFormat = sndfile_dyn::SF_FORMAT_WAV | (isFloat ? sndfile_dyn::SF_FORMAT_FLOAT : sndfile_dyn::SF_FORMAT_PCM_16);
            break;
        case AudioExportSettings::FLAC:
            // Map "Float" to 24-bit PCM for FLAC (highest supported lossless standard)
            dstFormat = sndfile_dyn::SF_FORMAT_FLAC | (isFloat ? sndfile_dyn::SF_FORMAT_PCM_24 : sndfile_dyn::SF_FORMAT_PCM_16);
            break;
        case AudioExportSettings::OGG_VORBIS:
            dstFormat = sndfile_dyn::SF_FORMAT_OGG | sndfile_dyn::SF_FORMAT_VORBIS;
            break;
        case AudioExportSettings::OPUS:
            dstFormat = sndfile_dyn::SF_FORMAT_OGG | sndfile_dyn::SF_FORMAT_OPUS;
            break;
        case AudioExportSettings::MP3:
            dstFormat = sndfile_dyn::SF_FORMAT_MPEG | sndfile_dyn::SF_FORMAT_MPEG_LAYER_III;
            break;
        default:
            sf_close(srcFile);
            return false;
    }

    // Setup destination file info
    sndfile_dyn::SF_INFO dstInfo = {};
    dstInfo.samplerate = srcInfo.samplerate;
    dstInfo.channels   = srcInfo.channels;
    dstInfo.format     = dstFormat;

    auto dstFile = openSndFile(outputPath, sndfile_dyn::SFM_WRITE, &dstInfo);
    if (!dstFile) {
        qWarning() << "FluidSynthEngine::transcodeWavTo: failed to open output file"
                    << outputPath
                    << "Format:" << QString::number(dstFormat, 16)
                    << "Error:" << (sf_strerror ? sf_strerror(nullptr) : "unknown error");
        sf_close(srcFile);
        return false;
    }
    qDebug() << "FluidSynthEngine::transcodeWavTo: output file opened successfully.";

    // Copy audio data in blocks, emitting progress for the transcode phase (80-100%)
    constexpr int BLOCK_FRAMES = 4096;
    std::vector<float> buffer(BLOCK_FRAMES * srcInfo.channels);
    sndfile_dyn::sf_count_t readCount;
    sndfile_dyn::sf_count_t totalFrames = srcInfo.frames;
    sndfile_dyn::sf_count_t framesProcessed = 0;
    int lastTranscodePercent = -1;

    while ((readCount = sf_readf_float(srcFile, buffer.data(), BLOCK_FRAMES)) > 0) {
        if (_exportCancelled.load()) {
            sf_close(dstFile);
            sf_close(srcFile);
            QFile::remove(outputPath);
            return false;
        }
        sf_writef_float(dstFile, buffer.data(), readCount);
        framesProcessed += readCount;

        // Report progress: transcode phase maps to 80-100%
        if (totalFrames > 0) {
            int transcodePercent = 80 + static_cast<int>(20.0 * static_cast<double>(framesProcessed) / totalFrames);
            if (transcodePercent > 100) transcodePercent = 100;
            
            if (transcodePercent != lastTranscodePercent) {
                lastTranscodePercent = transcodePercent;
                emit exportProgress(transcodePercent);
            }
        } else {
            // If totalFrames is not available, just emit 99% to show activity
            if (lastTranscodePercent < 100) {
                lastTranscodePercent = 100;
                emit exportProgress(100);
            }
        }
    }

    sf_close(dstFile);
    sf_close(srcFile);

    qDebug() << "FluidSynthEngine::transcodeWavTo: successfully transcoded to" << outputPath;
    return true;
}

// ============================================================================
// Audio Settings
// ============================================================================

void FluidSynthEngine::setAudioDriver(const QString &driver) {
    QMutexLocker locker(&_engineMutex);
    if (_audioDriverName == driver) return;
    
    _audioDriverName = driver;
    if (_initialized && _audioDriver) {
        // Driver hot-swap: destroy audio driver and recreate WITHOUT destroying synth or soundfonts
        delete_fluid_audio_driver(_audioDriver);
        fluid_settings_setstr(_settings, "audio.driver", driver.toUtf8().constData());
        _audioDriver = new_fluid_audio_driver(_settings, _synth);
        if (!_audioDriver) {
            qWarning() << "FluidSynth: Failed to switch to audio driver" << driver;
        } else {
            qDebug() << "FluidSynth: Hot-swapped audio driver to" << driver;
        }
    }
}

void FluidSynthEngine::setGain(double gain) {
    QMutexLocker locker(&_engineMutex);
    _gain = gain;
    if (_initialized && _synth) {
        fluid_synth_set_gain(_synth, static_cast<float>(gain));
    }
}

void FluidSynthEngine::setSampleRate(double rate) {
    QMutexLocker locker(&_engineMutex);
    if (_sampleRate == rate) return;
    
    _sampleRate = rate;
    if (_initialized) {
        // Sample rate requires recreating synth and reloading soundfonts
        if (!_isEngineRestartPending) {
            _isEngineRestartPending = true;
            QThreadPool::globalInstance()->start([this]() {
                QMutexLocker locker(&_engineMutex);
                _isEngineRestartPending = false;
                shutdown();
                initialize();
                QMetaObject::invokeMethod(this, "engineRestarted", Qt::QueuedConnection);
            });
        }
    }
}

void FluidSynthEngine::setReverbEngine(const QString &engine) {
    QMutexLocker locker(&_engineMutex);
    if (_reverbEngine == engine) return;
    
    _reverbEngine = engine;
    if (_initialized && _settings) {
        // FluidSynth allows changing reverb engine on the fly
        fluid_settings_setstr(_settings, "synth.reverb.engine", _reverbEngine.toUtf8().constData());
    }
}

void FluidSynthEngine::setReverbEnabled(bool enabled) {
    QMutexLocker locker(&_engineMutex);
    _reverbEnabled = enabled;
    if (_initialized && _synth) {
        fluid_synth_reverb_on(_synth, -1, enabled ? 1 : 0);
    }
}

void FluidSynthEngine::setChorusEnabled(bool enabled) {
    QMutexLocker locker(&_engineMutex);
    _chorusEnabled = enabled;
    if (_initialized && _synth) {
        fluid_synth_chorus_on(_synth, -1, enabled ? 1 : 0);
    }
}

void FluidSynthEngine::setPolyphony(int polyphony) {
    QMutexLocker locker(&_engineMutex);
    if (_polyphony != polyphony) {
        _polyphony = polyphony;
        if (_initialized && _synth) {
            fluid_synth_set_polyphony(_synth, _polyphony);
        }
    }
}

void FluidSynthEngine::setSampleFormat(const QString &format) {
    QMutexLocker locker(&_engineMutex);
    if (_sampleFormat != format) {
        _sampleFormat = format;
        if (_initialized) {
            if (!_isEngineRestartPending) {
                _isEngineRestartPending = true;
                QThreadPool::globalInstance()->start([this]() {
                    QMutexLocker locker(&_engineMutex);
                    _isEngineRestartPending = false;
                    shutdown();
                    initialize();
                    QMetaObject::invokeMethod(this, "engineRestarted", Qt::QueuedConnection);
                });
            }
        }
    }
}

QString FluidSynthEngine::audioDriver() const {
    return _audioDriverName;
}

double FluidSynthEngine::gain() const {
    return _gain;
}

double FluidSynthEngine::sampleRate() const {
    return _sampleRate;
}

QString FluidSynthEngine::reverbEngine() const {
    return _reverbEngine;
}

bool FluidSynthEngine::reverbEnabled() const {
    return _reverbEnabled;
}

bool FluidSynthEngine::chorusEnabled() const {
    return _chorusEnabled;
}

int FluidSynthEngine::polyphony() const {
    return _polyphony;
}

QString FluidSynthEngine::sampleFormat() const {
    return _sampleFormat;
}

QStringList FluidSynthEngine::availableAudioDrivers() const {
    QStringList drivers;
    fluid_settings_t *tmpSettings = new_fluid_settings();
    if (tmpSettings) {
        fluid_settings_foreach_option(tmpSettings, "audio.driver",
            &drivers,
            [](void *data, const char * /*name*/, const char *option) {
                QStringList *list = static_cast<QStringList*>(data);
                QString driverName = QString::fromUtf8(option);
                // Filter out the "file" driver (not useful for real-time playback)
                if (driverName != QLatin1String("file")) {
                    list->append(driverName);
                }
            });
        delete_fluid_settings(tmpSettings);
    }
    
    // Move WASAPI to the top if present
    int wasapiIdx = drivers.indexOf("wasapi");
    if (wasapiIdx > 0) {
        drivers.move(wasapiIdx, 0);
    }
    
    return drivers;
}

QString FluidSynthEngine::audioDriverDisplayName(const QString &driver) {
    static const QMap<QString, QString> displayNames = {
        {"dsound", "DirectSound"},
        {"wasapi", "WASAPI"},
        {"waveout", "WaveOut"},
        {"pulseaudio", "PulseAudio"},
        {"alsa", "ALSA"},
        {"jack", "JACK"},
        {"coreaudio", "CoreAudio"},
        {"portaudio", "PortAudio"},
        {"sdl2", "SDL2"},
        {"pipewire", "PipeWire"},
        {"opensles", "OpenSL ES"},
        {"oboe", "Oboe"}
    };
    return displayNames.value(driver, driver);
}

bool FluidSynthEngine::isSoundFontLoaded(const QString &path) const {
    QMutexLocker locker(&_engineMutex);
    QString canonicalPath = QFileInfo(path).canonicalFilePath();
    for (const auto &pair : _loadedFonts) {
        if (QFileInfo(pair.second).canonicalFilePath() == canonicalPath) {
            return true;
        }
    }
    // Also check pending paths
    for (const QString &pending : _pendingStackUpdate) {
        if (QFileInfo(pending).canonicalFilePath() == canonicalPath) {
            return true;
        }
    }
    return false;
}

QList<QPair<QString, bool>> FluidSynthEngine::soundFontCollection() const {
    QMutexLocker locker(&_engineMutex);
    return _collection;
}

// ============================================================================
// Persistence
// ============================================================================

void FluidSynthEngine::saveSettings(QSettings *settings) {
    settings->beginGroup("FluidSynth");

    settings->setValue("audioDriver", _audioDriverName);
    settings->setValue("gain", _gain);
    settings->setValue("sampleRate", _sampleRate);
    settings->setValue("sampleFormat", _sampleFormat);
    settings->setValue("reverbEngine", _reverbEngine);
    settings->setValue("polyphony", _polyphony);
    settings->setValue("reverbEnabled", _reverbEnabled);
    settings->setValue("chorusEnabled", _chorusEnabled);

    // Save SoundFont collection
    // Uses soundFontCollection() which works even when the engine is shut down
    QList<QPair<QString, bool>> collection = soundFontCollection();
    
    // Explicitly remove old collection to ensure a clean write (especially if empty)
    settings->remove("soundFontCollection");
    
    settings->beginWriteArray("soundFontCollection", collection.size());
    for (int i = 0; i < collection.size(); ++i) {
        settings->setArrayIndex(i);
        settings->setValue("path", collection[i].first);
        settings->setValue("enabled", collection[i].second);
    }
    settings->endArray();

    settings->endGroup();
}

void FluidSynthEngine::loadSettings(QSettings *settings) {
    settings->beginGroup("FluidSynth");

    _audioDriverName = settings->value("audioDriver", "wasapi").toString();
    _sampleRate = settings->value("sampleRate", 48000.0).toDouble();
    _sampleFormat = settings->value("sampleFormat", "16bits").toString();
    _gain = settings->value("gain", 0.5).toDouble();
    _reverbEngine = settings->value("reverbEngine", "fdn").toString();
    _reverbEnabled = settings->value("reverbEnabled", true).toBool();
    _chorusEnabled = settings->value("chorusEnabled", true).toBool();
    _polyphony = settings->value("polyphony", 256).toInt();

    int sfCount = settings->beginReadArray("soundFontCollection");
    QList<QPair<QString, bool>> collection;
    
    if (sfCount > 0) {
        for (int i = 0; i < sfCount; ++i) {
            settings->setArrayIndex(i);
            QString path = settings->value("path").toString();
            bool enabled = settings->value("enabled", true).toBool();
            
            // Auto-disable if file is missing
            if (enabled && !QFileInfo::exists(path)) {
                enabled = false;
                qDebug() << "FluidSynth: Disabling missing SoundFont on startup:" << path;
            }
            collection.append(qMakePair(path, enabled));
        }
    }
    settings->endArray();

    settings->endGroup();

    // Store paths directly
    _collection = collection;

    // If we're already initialized, reload SoundFonts immediately
    if (_initialized && !_collection.isEmpty()) {
        setSoundFontCollection(_collection);
    }
}

#endif // FLUIDSYNTH_SUPPORT
