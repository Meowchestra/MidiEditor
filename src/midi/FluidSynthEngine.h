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

#ifndef FLUIDSYNTHENGINE_H_
#define FLUIDSYNTHENGINE_H_

#ifdef FLUIDSYNTH_SUPPORT

#include <QObject>
#include <QList>
#include <QPair>
#include <QRecursiveMutex>
#include <QString>
#include <QStringList>

#include <fluidsynth.h>

class QSettings;

/**
 * \class FluidSynthEngine
 *
 * \brief Singleton managing the FluidSynth software synthesizer lifecycle.
 *
 * FluidSynthEngine provides a built-in MIDI synthesizer using FluidSynth,
 * eliminating the need for an external softsynth. It manages:
 *
 * - **Synthesizer lifecycle**: Creation and teardown of FluidSynth instances
 * - **SoundFont management**: Loading, unloading, and priority-based stacking
 * - **MIDI routing**: Parsing raw MIDI bytes and dispatching to FluidSynth
 * - **Audio configuration**: Driver, gain, sample rate, reverb, chorus
 * - **Settings persistence**: Save/restore configuration via QSettings
 *
 * SoundFont Stacking:
 * FluidSynth uses a stack-based priority system. The last-loaded SoundFont
 * has highest priority (checked first for instrument presets). This engine
 * exposes that as a reorderable list where top = highest priority.
 */
class FluidSynthEngine : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Returns the singleton instance.
     */
    static FluidSynthEngine* instance();

    // === Lifecycle ===

    /**
     * \brief Initializes the synthesizer, settings, and audio driver.
     * \return True if initialization was successful
     */
    bool initialize();

    /**
     * \brief Shuts down the audio driver, synth, and settings.
     */
    void shutdown();

    /**
     * \brief Returns true if the engine is initialized and ready.
     */
    bool isInitialized() const;

    // === SoundFont Management ===

    /**
     * \brief Loads a SoundFont file.
     * \param path Absolute path to an SF2 or SF3 file
     * \return FluidSynth sfont_id on success, -1 on error
     */
    int loadSoundFont(const QString &path);

    /**
     * \brief Sets the persistent collection of soundfonts and updates the stack.
     * \param collection List of soundfonts: Pair of (absolute_path, is_enabled)
     */
    void setSoundFontCollection(const QList<QPair<QString, bool>> &collection);

    /**
     * \brief Returns the full configured collection.
     */
    QList<QPair<QString, bool>> soundFontCollection() const;

    /**
     * \brief Unloads a SoundFont by its ID.
     * \param sfontId The FluidSynth sfont_id returned by loadSoundFont
     * \return True if unloaded successfully
     */
    bool unloadSoundFont(int sfontId);

    /**
     * \brief Unloads all loaded SoundFonts.
     */
    void unloadAllSoundFonts();

    /**
     * \brief Returns the currently loaded SoundFonts in priority order.
     * \return List of (sfont_id, file_path) pairs, highest priority first
     */
    QList<QPair<int, QString>> loadedSoundFonts() const;

    /**
     * \brief Replaces the entire SoundFont stack with the given paths.
     *
     * Unloads all current SoundFonts and reloads in the given order.
     * The last item in the list will have the highest priority in FluidSynth
     * (loaded last). The UI presents top = highest priority, so this method
     * loads items in reverse order.
     *
     * \param paths List of SF2/SF3 file paths, top = highest priority
     */
    void setSoundFontStack(const QStringList &paths);

    /**
     * \brief Adds new SoundFonts to the collection without enabling them by default.
     * \param paths List of absolute paths
     */
    void addSoundFonts(const QStringList &paths);

    // === MIDI Message Routing ===

    /**
     * \brief Routes raw MIDI bytes to the FluidSynth synthesizer.
     * \param data Raw MIDI message bytes
     */
    void sendMidiData(const QByteArray &data);

    // === Audio Settings ===

    void setAudioDriver(const QString &driver);
    void setGain(double gain);
    void setSampleRate(double rate);
    void setReverbEngine(const QString &engine);
    void setReverbEnabled(bool enabled);
    void setChorusEnabled(bool enabled);

    QString audioDriver() const;
    double gain() const;
    double sampleRate() const;
    QString reverbEngine() const;
    bool reverbEnabled() const;
    bool chorusEnabled() const;
    QStringList availableAudioDrivers() const;

    /**
     * \brief Returns a human-readable display name for an audio driver.
     */
    static QString audioDriverDisplayName(const QString &driver);

    /**
     * \brief Checks if a SoundFont with the same path is already loaded.
     */
    bool isSoundFontLoaded(const QString &path) const;

    /**
     * \brief Removes all references to soundFontPaths
     */

    /**
     * \brief Detects the default sample rate for a given audio driver.
     * \param driver Audio driver name (e.g. "wasapi", "dsound")
     * \return Default sample rate, or 44100.0 if detection fails
     */
    static double detectDefaultSampleRate(const QString &driver);

    // === Persistence ===

    /**
     * \brief Saves all FluidSynth settings to QSettings.
     */
    void saveSettings(QSettings *settings);

    /**
     * \brief Loads all FluidSynth settings from QSettings.
     */
    void loadSettings(QSettings *settings);

signals:
    void soundFontLoaded(int sfontId, const QString &path);
    void soundFontUnloaded(int sfontId);
    void soundFontsChanged();
    void initializationFailed(const QString &error);
    void engineRestarted();

private:
    FluidSynthEngine();
    ~FluidSynthEngine();

    // Non-copyable
    FluidSynthEngine(const FluidSynthEngine &) = delete;
    FluidSynthEngine &operator=(const FluidSynthEngine &) = delete;

    fluid_settings_t *_settings;
    fluid_synth_t *_synth;
    fluid_audio_driver_t *_audioDriver;
    bool _initialized;

    // Mutex protecting all fluid_* calls and engine state changes cross-thread
    mutable QRecursiveMutex _engineMutex;

    // SoundFont tracking: sfont_id → file path (in load order, last = highest priority)
    QList<QPair<int, QString>> _loadedFonts;

    // SoundFont collection (persisted)
    QList<QPair<QString, bool>> _collection;
    
    // Cached settings values
    QString _audioDriverName;
    QString _reverbEngine;
    double _gain;
    double _sampleRate;
    bool _reverbEnabled;
    bool _chorusEnabled;
    
    // Background task debouncing
    bool _isStackUpdatePending;
    QStringList _pendingStackUpdate;
    bool _isEngineRestartPending;
};

#endif // FLUIDSYNTH_SUPPORT

#endif // FLUIDSYNTHENGINE_H_
