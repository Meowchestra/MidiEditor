#include "Metronome.h"

#include "MidiFile.h"

#include <QFileInfo>
#include <QFileInfo>
#include "../midi/MidiOutput.h"

Metronome *Metronome::_instance = nullptr;
bool Metronome::_enable = false;

Metronome::Metronome(QObject *parent) : QObject(parent) {
    _file = 0;
    num = 4;
    denom = 2;
    loudness_cache = 100;
}

Metronome::~Metronome() {

}

void Metronome::setFile(MidiFile *file) {
    _file = file;
}

void Metronome::measureUpdate(int measure, int tickInMeasure) {
    // compute pos
    if (!_file) {
        return;
    }

    int ticksPerClick = (_file->ticksPerQuarter() * 4) / qPow(2, denom);
    int pos = tickInMeasure / ticksPerClick;

    if (lastMeasure < measure) {
        click(true); // Downbeat
        lastMeasure = measure;
        lastPos = 0;
        return;
    } else {
        if (pos > lastPos) {
            click(false); // Regular beat
            lastPos = pos;
            return;
        }
    }
}

void Metronome::meterChanged(int n, int d) {
    num = n;
    denom = d;
}

void Metronome::playbackStarted() {
    reset();
}

void Metronome::playbackStopped() {
}

Metronome *Metronome::instance() {
    if (!_instance) {
        _instance = new Metronome();
    }
    return _instance;
}

void Metronome::reset() {
    lastPos = 0;
    lastMeasure = -1;
}

void Metronome::click(bool isDownbeat) {
    if (!enabled()) {
        return;
    }

    if (MidiOutput::isConnected()) {
        QByteArray event;
        event.resize(3);
        // Note On, Channel 10 (drums)
        // Note 76 = High Wood Block, Note 77 = Low Wood Block
        // (Much punchier and cuts through the mix better than standard click)
        event[1] = (char)(isDownbeat ? 76 : 77);
        // Velocity (volume) - maximizing to make it audible
        event[2] = (char)(qMin(127, (int)(loudness() * 1.27))); // scale 0-100 to 0-127
        
        // Send command multiple times simultaneously to mechanically amplify polyphony 
        // to prevent being drowned out
        MidiOutput::sendCommand(event);
        MidiOutput::sendCommand(event);
        MidiOutput::sendCommand(event);
        
        // Note Off follows immediately for click sounds
        event[0] = (char)0x89;
        event[2] = 0;
        MidiOutput::sendCommand(event);
    }
}

bool Metronome::enabled() {
    return _enable;
}

void Metronome::setEnabled(bool b) {
    _enable = b;
}

void Metronome::setLoudness(int value) {
    if (_instance) {
        _instance->loudness_cache = value;
    }
}

int Metronome::loudness() {
    if (_instance) {
        return _instance->loudness_cache;
    }
    return 100;
}
