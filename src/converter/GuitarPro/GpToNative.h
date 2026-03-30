#ifndef GPTONATIVE_H
#define GPTONATIVE_H

#include "GpModels.h"
#include "GpMidiExport.h"
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <optional>

// ============================================================
// Native bend/tremolo points — ported from Native/BendPoint.cs, Native/TremoloPoint.cs
// ============================================================

struct NativeBendPoint {
    int index = 0;
    float value = 0.0f;
    int usedChannel = -1;

    NativeBendPoint() = default;
    NativeBendPoint(int idx, float val, int ch = -1)
        : index(idx), value(val), usedChannel(ch) {}
};

struct NativeTremoloPoint {
    int index = 0;
    float value = 0.0f;

    NativeTremoloPoint() = default;
    NativeTremoloPoint(int idx, float val) : index(idx), value(val) {}
};

// ============================================================
// Native note — ported from Native/Note.cs
// ============================================================

struct NativeNote {
    std::vector<NativeBendPoint> bendPoints;
    bool connect = false;
    int duration = 0;
    Fading fading = Fading::None;
    int fret = 0;
    HarmonicType harmonic = HarmonicType::None;
    float harmonicFret = 0.0f;
    int index = 0;
    bool isHammer = false;
    bool isMuted = false;
    bool isPalmMuted = false;
    bool isPopped = false;
    bool isRhTapped = false;
    bool isSlapped = false;
    bool isTremBarVibrato = false;
    bool isVibrato = false;
    float resizeValue = 1.0f;
    bool slideInFromAbove = false;
    bool slideInFromBelow = false;
    bool slideOutDownwards = false;
    bool slideOutUpwards = false;
    bool slidesToNext = false;
    int str = 0;
    std::optional<int> midiNote; // Direct MIDI note value from GP8+ files
    int velocity = 100;

    void addBendPoints(const std::vector<NativeBendPoint>& points) {
        bendPoints.insert(bendPoints.end(), points.begin(), points.end());
    }
};

// ============================================================
// BendingPlan — ported from Native/BendingPlan.cs
// ============================================================

struct NativeBendingPlan {
    int originalChannel;
    int usedChannel;
    std::vector<NativeBendPoint> bendingPoints;

    NativeBendingPlan(int origCh, int usedCh, std::vector<NativeBendPoint> points)
        : originalChannel(origCh), usedChannel(usedCh), bendingPoints(std::move(points)) {}

    static NativeBendingPlan create(std::vector<NativeBendPoint> bendPoints,
        int originalChannel, int usedChannel,
        int duration, int index, float resize, bool isVibrato);
};

// ============================================================
// NativeMasterBar — ported from Native/MasterBar.cs
// ============================================================

struct NativeMasterBar {
    int den = 4;
    int duration = 0;
    int index = 0;     // Midi Index — filled during retrieveNotes
    int key = 0;       // C, -1 = F, 1 = G
    std::string keyBoth = "0";
    int keyType = 0;   // 0 = Major, 1 = Minor
    int num = 4;
    std::string time = "4/4";
    TripletFeel tripletFeel = TripletFeel::none;
};

// ============================================================
// NativeTrack — ported from Native/Track.cs
// ============================================================

struct NativeTrack {
    int capo = 0;
    int channel = 0;
    std::string name;
    std::vector<NativeNote> notes;
    int patch = 0;
    PlaybackState state = PlaybackState::Def;
    int port = 0;
    std::vector<NativeTremoloPoint> tremoloPoints;
    std::vector<int> tuning = {40, 45, 50, 55, 59, 64};

    std::unique_ptr<GpMidiTrack> getMidi(bool availableChannels[16]);

private:
    static int tryToFindChannel(const bool availableChannels[16]);
    static int getHarmonic(int baseTone, int fret, int capo,
                           float harmonicFret, HarmonicType type);
    static std::vector<NativeBendPoint> findAndSortCurrentBendPoints(
        const std::vector<NativeBendingPlan>& plans, int index);
    static std::vector<NativeTremoloPoint> addDetailsToTremoloPoints(
        const std::vector<NativeTremoloPoint>& points, int maxDistance);
    static std::vector<std::array<int,2>> createVolumeChanges(
        int index, int duration, int velocity, Fading fading);
    static std::vector<int> getActiveChannels(
        int trackChannel, const std::vector<std::array<int,3>>& channelConnections);
};

// ============================================================
// NativeFormat — ported from Native/Format.cs
// ============================================================

class NativeFormat {
public:
    explicit NativeFormat(GpFile* gpFile);
    GpMidiExport toMidi();

    // Per-instance channel availability (thread-safe; replaces C# static)
    bool availableChannels[16] = {};

private:
    GpFile* gpFile_;
    std::string title_, subtitle_, artist_, album_, words_, music_;

    struct NativeTempo {
        int position = 0;
        float value = 120.0f;
    };

    std::vector<NativeTempo> tempos_;
    std::vector<NativeMasterBar> masterBars_;
    std::vector<NativeTrack> nativeTracks_;
    std::vector<int> notesInMeasures_;

    std::vector<NativeTempo> retrieveTempos();
    std::vector<NativeMasterBar> retrieveMasterBars();
    std::vector<NativeTrack> retrieveTracks();
    std::vector<NativeNote> retrieveNotes(const GpTrack& track,
        const std::vector<int>& tuning, NativeTrack& myTrack);

    void updateAvailableChannels();
    std::unique_ptr<GpMidiTrack> getMidiHeader();

    static int flipDuration(const Duration& dur);
    static void addToTremoloBarList(int index, int duration,
        const BendEffect& bend, NativeTrack& myTrack);
    static std::vector<NativeBendPoint> getBendPoints(int index,
        int duration, const BendEffect& bend);
    static std::vector<int> getTuning(const std::vector<GuitarString>& strings);
};

#endif // GPTONATIVE_H
