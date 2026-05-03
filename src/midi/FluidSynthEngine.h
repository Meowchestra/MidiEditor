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
#include <atomic>

#include <fluidsynth.h>

/**
 * \brief Configuration for audio export operations.
 */
struct AudioExportSettings {
    enum Format { WAV, FLAC, OPUS, OGG_VORBIS, MP3 };

    Format format = WAV;
    int rate = 48000;
    QString sampleFormat = "16bits";
    bool includeReverbTail = true;  ///< Add 2s after last note for reverb/sustain fade

    double sampleRate() const { return static_cast<double>(rate); }

    /// FluidSynth audio.file.type value
    QString fileType() const {
        switch (format) {
            case WAV:        return "wav";
            case FLAC:       return "flac";
            case OPUS:       return "opus";
            case OGG_VORBIS: return "ogg";
            case MP3:        return "mp3";
        }
        return "wav";
    }

    /// File dialog filter string
    QString filterString() const {
        switch (format) {
            case WAV:        return QObject::tr("WAV Audio (*.wav)");
            case FLAC:       return QObject::tr("FLAC Audio (*.flac)");
            case OPUS:       return QObject::tr("Opus Audio (*.opus)");
            case OGG_VORBIS: return QObject::tr("OGG Vorbis Audio (*.ogg)");
            case MP3:        return QObject::tr("MP3 Audio (*.mp3)");
        }
        return QObject::tr("WAV Audio (*.wav)");
    }

    /// Default file extension including dot
    QString extension() const {
        switch (format) {
            case WAV:        return ".wav";
            case FLAC:       return ".flac";
            case OPUS:       return ".opus";
            case OGG_VORBIS: return ".ogg";
            case MP3:        return ".mp3";
        }
        return ".wav";
    }

    /// Human-readable format name
    static QString formatName(Format f) {
        switch (f) {
            case WAV:        return QObject::tr("WAV - Uncompressed PCM (.wav)");
            case FLAC:       return QObject::tr("FLAC - Lossless Compression (.flac)");
            case OPUS:       return QObject::tr("Opus - Lossy Compression (.opus)");
            case OGG_VORBIS: return QObject::tr("OGG Vorbis - Lossy Compression (.ogg)");
            case MP3:        return QObject::tr("MP3 - Lossy Compression (.mp3)");
        }
        return "WAV";
    }

    /// Short format name (e.g. WAV, Opus)
    static QString formatShortName(Format f) {
        switch (f) {
            case WAV:        return "WAV";
            case FLAC:       return "FLAC";
            case OPUS:       return "Opus";
            case OGG_VORBIS: return "OGG Vorbis";
            case MP3:        return "MP3";
        }
        return "WAV";
    }
};

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
     * \param path Absolute path to an SF2, SF3, or DLS file
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
     * \param paths List of SF2/SF3/DLS file paths, top = highest priority
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

    // === Export ===

    /**
     * \brief Multi-format audio export. Supports WAV, FLAC, Opus, OGG Vorbis, MP3.
     *
     * Creates an offline FluidSynth instance, renders the MIDI file to
     * the specified output path using the given settings.
     * Emits exportProgress/exportFinished/exportCancelled signals.
     *
     * \param midiFilePath Path to the temporary MIDI file to render
     * \param outputPath Path for the output audio file
     * \param settings Export configuration (format, quality, reverb tail)
     * \param totalTicks Used for progress reporting
     */
    void exportAudio(const QString &midiFilePath, const QString &outputPath,
                     const AudioExportSettings &settings, int totalTicks);

    /**
     * \brief Requests cancellation of a running export.
     */
    void cancelExport();

    // === Audio Settings ===
    void setAudioDriver(const QString &driver);
    void setGain(double gain);
    void setSampleRate(double rate);
    void setSampleFormat(const QString &format);
    void setReverbEngine(const QString &engine);
    void setReverbEnabled(bool enabled);
    void setChorusEnabled(bool enabled);
    void setPolyphony(int polyphony);

    QString audioDriver() const;
    double gain() const;
    double sampleRate() const;
    QString sampleFormat() const;
    QString reverbEngine() const;
    bool reverbEnabled() const;
    bool chorusEnabled() const;
    int polyphony() const;

    QStringList availableAudioDrivers() const;

    /**
     * \brief Returns a human-readable display name for an audio driver.
     */
    static QString audioDriverDisplayName(const QString &driver);

    /**
     * \brief Checks if a SoundFont with the same path is already loaded.
     */
    bool isSoundFontLoaded(const QString &path) const;

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
    void exportProgress(int percent);
    void exportFinished(bool success, const QString &path);
    void exportCancelled();

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
    int _polyphony;
    QString _sampleFormat;
    
    // Background task debouncing
    bool _isStackUpdatePending;
    QStringList _pendingStackUpdate;
    bool _isEngineRestartPending;

    // Export cancellation flag
    std::atomic<bool> _exportCancelled{false};

    /// Returns true for formats that FluidSynth can't render directly (Opus, MP3)
    static bool needsTranscode(const AudioExportSettings &settings);

    /// Transcodes a rendered WAV file to the target format using libsndfile.
    /// Returns true on success.
    bool transcodeWavTo(const QString &wavPath, const QString &outputPath,
                        const AudioExportSettings &settings);
};

#endif // FLUIDSYNTH_SUPPORT

#endif // FLUIDSYNTHENGINE_H_
