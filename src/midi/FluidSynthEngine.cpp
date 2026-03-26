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
      _reverbEngine("fdn"),
      _gain(0.5),
      _sampleRate(44100.0),
      _reverbEnabled(true),
      _chorusEnabled(true) {
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
    // Set custom reverb engine only if supported (wrap string in literal or standard way to avoid errors if fallback is used)
    fluid_settings_setstr(_settings, "synth.reverb.engine", _reverbEngine.toUtf8().constData());

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
    qDebug() << "  Audio driver:" << _audioDriverName;
    qDebug() << "  Sample rate:" << _sampleRate;
    qDebug() << "  Gain:" << _gain;

    // Load any pending SoundFonts from saved settings
    if (!_pendingSoundFontPaths.isEmpty()) {
        QThreadPool::globalInstance()->start([this, paths = _pendingSoundFontPaths]() {
            QMutexLocker lock(&_engineMutex);
            setSoundFontStack(paths);
            QMetaObject::invokeMethod(this, "soundFontsChanged", Qt::QueuedConnection);
        });
        _pendingSoundFontPaths.clear();
    }

    return true;
}

void FluidSynthEngine::shutdown() {
    QMutexLocker locker(&_engineMutex);
    
    if (!_initialized) {
        return;
    }

    // Preserve SoundFont paths so they survive engine shutdown
    if (_pendingSoundFontPaths.isEmpty() && !_loadedFonts.isEmpty()) {
        _pendingSoundFontPaths.clear();
        // Store in priority order (highest first = last loaded first)
        for (int i = _loadedFonts.size() - 1; i >= 0; --i) {
            _pendingSoundFontPaths.append(_loadedFonts[i].second);
        }
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

void FluidSynthEngine::addSoundFonts(const QStringList &paths) {
    QThreadPool::globalInstance()->start([this, paths]() {
        QMutexLocker locker(&_engineMutex);
        for (const QString &path : paths) {
            loadSoundFont(path);
        }
        QMetaObject::invokeMethod(this, "soundFontsChanged", Qt::QueuedConnection);
    });
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
    for (int i = 0; i < _pendingSoundFontPaths.size(); ++i) {
        // Use negative IDs as placeholders since there's no real sfont_id
        result.append(qMakePair(-(i + 1), _pendingSoundFontPaths[i]));
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

    // Unload all current SoundFonts
    unloadAllSoundFonts();

    // Load in reverse order so that the first item (highest UI priority) is loaded last
    // (FluidSynth checks the most-recently-loaded SoundFont first)
    for (int i = paths.size() - 1; i >= 0; --i) {
        loadSoundFont(paths[i]);
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
        QThreadPool::globalInstance()->start([this]() {
            QMutexLocker locker(&_engineMutex);
            shutdown();
            initialize();
            QMetaObject::invokeMethod(this, "engineRestarted", Qt::QueuedConnection);
        });
    }
}

void FluidSynthEngine::setReverbEngine(const QString &engine) {
    QMutexLocker locker(&_engineMutex);
    if (_reverbEngine == engine) return;
    
    _reverbEngine = engine;
    if (_initialized) {
        // Reverb engine requires recreating synth and reloading soundfonts
        QThreadPool::globalInstance()->start([this]() {
            QMutexLocker locker(&_engineMutex);
            shutdown();
            initialize();
            QMetaObject::invokeMethod(this, "engineRestarted", Qt::QueuedConnection);
        });
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
    for (const QString &pending : _pendingSoundFontPaths) {
        if (QFileInfo(pending).canonicalFilePath() == canonicalPath) {
            return true;
        }
    }
    return false;
}

QStringList FluidSynthEngine::soundFontPaths() const {
    QMutexLocker locker(&_engineMutex);
    if (_initialized) {
        QStringList paths;
        // Return in priority order (highest first = last loaded first)
        for (int i = _loadedFonts.size() - 1; i >= 0; --i) {
            paths.append(_loadedFonts[i].second);
        }
        return paths;
    }
    return _pendingSoundFontPaths;
}

double FluidSynthEngine::detectDefaultSampleRate(const QString &driver) {
    fluid_settings_t *tmpSettings = new_fluid_settings();
    if (!tmpSettings) {
        return 44100.0;
    }

    if (!driver.isEmpty()) {
        fluid_settings_setstr(tmpSettings, "audio.driver", driver.toUtf8().constData());
    }

    double defaultRate = 44100.0;
    fluid_settings_getnum_default(tmpSettings, "synth.sample-rate", &defaultRate);

    delete_fluid_settings(tmpSettings);
    return defaultRate;
}

// ============================================================================
// Persistence
// ============================================================================

void FluidSynthEngine::saveSettings(QSettings *settings) {
    settings->beginGroup("FluidSynth");

    settings->setValue("audioDriver", _audioDriverName);
    settings->setValue("reverbEngine", _reverbEngine);
    settings->setValue("gain", _gain);
    settings->setValue("sampleRate", _sampleRate);
    settings->setValue("reverbEnabled", _reverbEnabled);
    settings->setValue("chorusEnabled", _chorusEnabled);

    // Save SoundFont paths in priority order (first = highest priority)
    // Uses soundFontPaths() which works even when the engine is shut down
    settings->setValue("soundFontPaths", soundFontPaths());

    settings->endGroup();
}

void FluidSynthEngine::loadSettings(QSettings *settings) {
    settings->beginGroup("FluidSynth");

    _audioDriverName = settings->value("audioDriver", "").toString();
    _reverbEngine = settings->value("reverbEngine", "fdn").toString();
    _gain = settings->value("gain", 0.5).toDouble();
    _reverbEnabled = settings->value("reverbEnabled", true).toBool();
    _chorusEnabled = settings->value("chorusEnabled", true).toBool();

    // Detect native sample rate if not previously saved
    if (settings->contains("sampleRate")) {
        _sampleRate = settings->value("sampleRate", 44100.0).toDouble();
    } else {
        _sampleRate = detectDefaultSampleRate(_audioDriverName);
        qDebug() << "FluidSynth: Auto-detected default sample rate:" << _sampleRate;
    }

    QStringList fontPaths = settings->value("soundFontPaths").toStringList();

    settings->endGroup();

    // Store paths for deferred loading when engine initializes
    _pendingSoundFontPaths = fontPaths;

    // If we're already initialized, reload SoundFonts immediately
    if (_initialized && !fontPaths.isEmpty()) {
        setSoundFontStack(fontPaths);
        _pendingSoundFontPaths.clear();
    }
}

#endif // FLUIDSYNTH_SUPPORT
