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
#include <QFileInfo>
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
      _audioDriverName(),
      _sampleRate(44100.0),
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

void FluidSynthEngine::exportToWav(const QString &midiFilePath, const QString &wavFilePath) {
    if (!_initialized) {
        emit exportFinished(false, wavFilePath);
        return;
    }

    fluid_settings_t* settings = new_fluid_settings();
    fluid_settings_setstr(settings, "audio.driver", "file");
    fluid_settings_setstr(settings, "audio.file.name", wavFilePath.toUtf8().constData());
    
    _engineMutex.lock();
    fluid_settings_setnum(settings, "synth.sample-rate", _sampleRate);
    fluid_settings_setint(settings, "synth.reverb.active", _reverbEnabled ? 1 : 0);
    fluid_settings_setint(settings, "synth.chorus.active", _chorusEnabled ? 1 : 0);
    fluid_settings_setnum(settings, "synth.gain", _gain);
    fluid_settings_setint(settings, "synth.polyphony", _polyphony);
    fluid_settings_setstr(settings, "audio.sample-format", _sampleFormat.toUtf8().constData());
    fluid_settings_setstr(settings, "synth.reverb.engine", _reverbEngine.toUtf8().constData());

    // Get loaded font paths while holding lock
    QStringList fontstoload;
    for (int i = 0; i < _loadedFonts.size(); ++i) {
        fontstoload.append(_loadedFonts[i].second);
    }
    _engineMutex.unlock();

    fluid_synth_t* synth = new_fluid_synth(settings);
    if (!synth) {
        delete_fluid_settings(settings);
        emit exportFinished(false, wavFilePath);
        return;
    }

    for (const QString &f : fontstoload) {
        fluid_synth_sfload(synth, f.toUtf8().constData(), 1);
    }

    fluid_player_t* player = new_fluid_player(synth);
    if (fluid_player_add(player, midiFilePath.toUtf8().constData()) != FLUID_OK) {
        delete_fluid_player(player);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        emit exportFinished(false, wavFilePath);
        return;
    }

    fluid_player_play(player);
    fluid_file_renderer_t* renderer = new_fluid_file_renderer(synth);
    if (!renderer) {
        delete_fluid_player(player);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        emit exportFinished(false, wavFilePath);
        return;
    }

    int totalTicks = fluid_player_get_total_ticks(player);
    if (totalTicks <= 0) totalTicks = 1; // guard
    
    int lastPercent = -1;

    while (fluid_player_get_status(player) == FLUID_PLAYER_PLAYING) {
        if (fluid_file_renderer_process_block(renderer) != FLUID_OK) {
            break;
        }
        int curTick = fluid_player_get_current_tick(player);
        int percent = (int)(100.0 * curTick / totalTicks);
        if (percent != lastPercent) {
            lastPercent = percent;
            emit exportProgress(percent);
        }
    }

    delete_fluid_file_renderer(renderer);
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    emit exportProgress(100);
    emit exportFinished(true, wavFilePath);
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
    settings->beginWriteArray("soundFontCollection");
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

    _audioDriverName = settings->value("audioDriver", "").toString();
    _sampleRate = settings->value("sampleRate", 44100.0).toDouble();
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
            collection.append(qMakePair(path, enabled));
        }
    } else {
        // Fallback for older versions that stored soundFontPaths
        QStringList fontPaths = settings->value("soundFontPaths").toStringList();
        for (const QString &p : fontPaths) {
            collection.append(qMakePair(p, true)); // Default to enabled
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
