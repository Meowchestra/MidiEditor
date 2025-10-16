#include "Metronome.h"

#include "MidiFile.h"

#include <QFileInfo>
#include <QSoundEffect>
#include <QUrl>

Metronome *Metronome::_instance = nullptr;
bool Metronome::_enable = false;

Metronome::Metronome(QObject *parent) : QObject(parent) {
    _file = 0;
    num = 4;
    denom = 2;
    _player = new QSoundEffect;
    _player->setVolume(1.0);

    // Try to load metronome sound file - silently fail if missing
    try {
        _player->setSource(QUrl::fromLocalFile(":/run_environment/metronome/metronome-01.wav"));
    } catch (...) {
        // Silently ignore - metronome will just be disabled
    }
}

Metronome::~Metronome() {
    if (_player) {
        delete _player;
        _player = nullptr;
    }
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
        click();
        lastMeasure = measure;
        lastPos = 0;
        return;
    } else {
        if (pos > lastPos) {
            click();
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

void Metronome::click() {
    if (!enabled()) {
        return;
    }

    // Only play if the audio file was loaded successfully
    if (_player && _player->status() != QSoundEffect::Error) {
        _player->play();
    }
}

bool Metronome::enabled() {
    return _enable;
}

void Metronome::setEnabled(bool b) {
    _enable = b;
}

void Metronome::setLoudness(int value) {
    if (_instance && _instance->_player) {
        _instance->_player->setVolume(value / 100.0);
    }
}

int Metronome::loudness() {
    if (_instance && _instance->_player) {
        return (int) (_instance->_player->volume() * 100);
    }
    return 100;
}
