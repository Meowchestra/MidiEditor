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

#include "TransposeDialog.h"
#include "MainWindow.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSet>
#include <QCheckBox>

#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/KeySignatureEvent.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"

static const char* ROOT_NOTES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct ScaleDef {
    const char* name;
    QList<int> pattern;
};

static QList<ScaleDef> getScales() {
    return {
        {"Major (Ionian)", {0, 2, 4, 5, 7, 9, 11}},
        {"Minor (Natural/Aeolian)", {0, 2, 3, 5, 7, 8, 10}},
        {"Minor (Harmonic)", {0, 2, 3, 5, 7, 8, 11}},
        {"Minor (Melodic/Jazz)", {0, 2, 3, 5, 7, 9, 11}},
        {"Pentatonic Major", {0, 2, 4, 7, 9}},
        {"Pentatonic Minor", {0, 3, 5, 7, 10}},
        {"Blues Scale", {0, 3, 5, 6, 7, 10}},
        {"Dorian (Jazz/Funk)", {0, 2, 3, 5, 7, 9, 10}},
        {"Phrygian (Spanish/Metal)", {0, 1, 3, 5, 7, 8, 10}},
        {"Lydian (Cinematic)", {0, 2, 4, 6, 7, 9, 11}},
        {"Mixolydian (Blues/Rock)", {0, 2, 4, 5, 7, 9, 10}},
        {"Locrian", {0, 1, 3, 5, 6, 8, 10}}
    };
}

TransposeDialog::TransposeDialog(QList<NoteOnEvent *> toTranspose, MidiFile *file, QWidget *parent)
    : QDialog(parent), _excludeDrums(nullptr) {
    setWindowTitle(tr("Transpose Selection"));
    setMinimumWidth(500);

    _toTranspose = toTranspose;
    _file = file;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Selection Summary
    QGroupBox *summaryGroup = new QGroupBox(tr("Selection Context"), this);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryGroup);
    _chromaticIntervalInfo = new QLabel(summaryGroup);
    _chromaticIntervalInfo->setStyleSheet("line-height: 1.4;");
    summaryLayout->addWidget(_chromaticIntervalInfo);
    mainLayout->addWidget(summaryGroup);

    // Method Selector
    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(tr("Method:"), this));
    _modeSelector = new QComboBox(this);
    _modeSelector->addItem(tr("Simple (Chromatic)"), 0);
    _modeSelector->addItem(tr("Change Key (Key-to-Key)"), 1);
    _modeSelector->addItem(tr("Scale-Aware (Diatonic)"), 2);
    _modeSelector->setMinimumHeight(28);
    modeLayout->addWidget(_modeSelector, 1);
    mainLayout->addLayout(modeLayout);

    _stack = new QStackedWidget(this);
    _stack->addWidget(createStandardTab());
    _stack->addWidget(createKeyTab());
    _stack->addWidget(createScaleTab());
    mainLayout->addWidget(_stack);
    connect(_modeSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_modeSelector, SIGNAL(currentIndexChanged(int)), _stack, SLOT(setCurrentIndex(int)));

    // Global Options
    QGroupBox *optionsGroup = new QGroupBox(tr("Global Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);
    optionsLayout->setSpacing(2);
    
    QCheckBox *protectRange = new QCheckBox(tr("Protect Range"), optionsGroup);
    protectRange->setChecked(true);
    optionsLayout->addWidget(protectRange);
    QLabel *protectDesc = new QLabel(tr("Prevents chords from collapsing by anchoring notes that hit the absolute top or bottom of the MIDI range."), optionsGroup);
    protectDesc->setWordWrap(true);
    protectDesc->setStyleSheet("font-size: 11px; margin-bottom: 8px; margin-left: 20px; color: #888;");
    optionsLayout->addWidget(protectDesc);

    _excludeDrums = new QCheckBox(tr("Exclude Drum Channel"), optionsGroup);
    _excludeDrums->setChecked(true);
    connect(_excludeDrums, SIGNAL(toggled(bool)), this, SLOT(updateIntervalLabel()));
    optionsLayout->addWidget(_excludeDrums);
    QLabel *drumsDesc = new QLabel(tr("Prevents percussion instruments from being accidentally pitch-shifted into entirely different drum sounds."), optionsGroup);
    drumsDesc->setWordWrap(true);
    drumsDesc->setStyleSheet("font-size: 11px; margin-left: 20px; color: #888;");
    optionsLayout->addWidget(drumsDesc);

    mainLayout->addWidget(optionsGroup);
    
    // Dialog Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *acceptButton = new QPushButton(tr("Accept"));
    acceptButton->setDefault(true);
    connect(acceptButton, &QPushButton::clicked, [this, protectRange](){ 
        this->_protectRange = protectRange->isChecked();
        this->accept(); 
    });
    
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(hide()));
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(acceptButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    updateIntervalLabel();
}

void TransposeDialog::detectScale(int &outRoot, int &outType) {
    if (_toTranspose.isEmpty()) { outRoot = 0; outType = 0; return; }
    QSet<int> pcs;
    foreach(NoteOnEvent* e, _toTranspose) pcs.insert(e->note() % 12);
    
    QList<ScaleDef> scales = getScales();
    int bestScore = -1;
    for (int r = 0; r < 12; r++) {
        for (int t = 0; t < scales.size(); t++) {
            int score = 0;
            QSet<int> scale;
            foreach(int p, scales[t].pattern) scale.insert((p + r) % 12);
            foreach(int pc, pcs) if (scale.contains(pc)) score++;
            if (score > bestScore) { bestScore = score; outRoot = r; outType = t; }
        }
    }
}

QString TransposeDialog::identifyRange() {
    if (_toTranspose.isEmpty()) return tr("None");
    int minN = 127, maxN = 0;
    foreach(NoteOnEvent* e, _toTranspose) {
        if (e->note() < minN) minN = e->note();
        if (e->note() > maxN) maxN = e->note();
    }
    auto noteName = [](int n) { return QString("%1%2").arg(ROOT_NOTES[qBound(0, n, 127) % 12]).arg(qBound(0, n, 127) / 12 - 1); };
    return tr("%1 to %2").arg(noteName(minN)).arg(noteName(maxN));
}

QString TransposeDialog::identifyChord() {
    if (_toTranspose.isEmpty()) return "";
    QSet<int> pitchClasses;
    foreach(NoteOnEvent* e, _toTranspose) pitchClasses.insert(e->note() % 12);
    if (pitchClasses.size() < 2 || pitchClasses.size() > 5) return "";
    for (int r = 0; r < 12; r++) {
        QSet<int> major = {r, (r + 4) % 12, (r + 7) % 12};
        QSet<int> minor = {r, (r + 3) % 12, (r + 7) % 12};
        if (pitchClasses == major) return QString("%1 Major").arg(ROOT_NOTES[r]);
        if (pitchClasses == minor) return QString("%1 Minor").arg(ROOT_NOTES[r]);
    }
    return "";
}

QWidget* TransposeDialog::createStandardTab() {
    QWidget *w = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    QGroupBox *box = new QGroupBox(tr("Chromatic Shift"), w);
    QGridLayout *g = new QGridLayout(box);

    _chromaticIntervalCombo = new QComboBox(box);
    _chromaticIntervalCombo->addItem(tr("Custom"), 0);
    _chromaticIntervalCombo->addItem(tr("Minor 2nd"), 1);
    _chromaticIntervalCombo->addItem(tr("Major 2nd"), 2);
    _chromaticIntervalCombo->addItem(tr("Minor 3rd"), 3);
    _chromaticIntervalCombo->addItem(tr("Major 3rd"), 4);
    _chromaticIntervalCombo->addItem(tr("Perfect 4th"), 5);
    _chromaticIntervalCombo->addItem(tr("Tritone"), 6);
    _chromaticIntervalCombo->addItem(tr("Perfect 5th"), 7);
    _chromaticIntervalCombo->addItem(tr("Minor 6th"), 8);
    _chromaticIntervalCombo->addItem(tr("Major 6th"), 9);
    _chromaticIntervalCombo->addItem(tr("Minor 7th"), 10);
    _chromaticIntervalCombo->addItem(tr("Major 7th"), 11);
    _chromaticIntervalCombo->addItem(tr("Octave"), 12);
    connect(_chromaticIntervalCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onIntervalComboChanged(int)));

    _chromaticOctBox = new QSpinBox(box);
    _chromaticOctBox->setRange(0, 10); _chromaticOctBox->setSuffix(tr(" Octave(s)"));
    connect(_chromaticOctBox, &QSpinBox::valueChanged, [this](int val) {
        int totalMs = val * 12 + _chromaticMsBox->value();
        _chromaticIntervalCombo->blockSignals(true);
        if (totalMs <= 12) _chromaticIntervalCombo->setCurrentIndex(totalMs);
        else _chromaticIntervalCombo->setCurrentIndex(0);
        _chromaticIntervalCombo->blockSignals(false);
        updateIntervalLabel();
    });

    _chromaticMsBox = new QSpinBox(box);
    _chromaticMsBox->setRange(-1, 12); _chromaticMsBox->setSuffix(tr(" Semitone(s)"));
    connect(_chromaticMsBox, &QSpinBox::valueChanged, [this](int val){
        if (val > 11) {
            if (_chromaticOctBox->value() < _chromaticOctBox->maximum()) {
                _chromaticMsBox->blockSignals(true);
                _chromaticMsBox->setValue(0);
                _chromaticMsBox->blockSignals(false);
                _chromaticOctBox->setValue(_chromaticOctBox->value() + 1);
            } else {
                _chromaticMsBox->blockSignals(true);
                _chromaticMsBox->setValue(11);
                _chromaticMsBox->blockSignals(false);
            }
        } else if (val < 0) {
            if (_chromaticOctBox->value() > _chromaticOctBox->minimum()) {
                _chromaticMsBox->blockSignals(true);
                _chromaticMsBox->setValue(11);
                _chromaticMsBox->blockSignals(false);
                _chromaticOctBox->setValue(_chromaticOctBox->value() - 1);
            } else {
                _chromaticMsBox->blockSignals(true);
                _chromaticMsBox->setValue(0);
                _chromaticMsBox->blockSignals(false);
            }
        }
        int totalMs = _chromaticOctBox->value() * 12 + _chromaticMsBox->value();
        _chromaticIntervalCombo->blockSignals(true);
        if (totalMs <= 12) _chromaticIntervalCombo->setCurrentIndex(totalMs);
        else _chromaticIntervalCombo->setCurrentIndex(0);
        _chromaticIntervalCombo->blockSignals(false);
        updateIntervalLabel();
    });

    _chromaticUp = new QRadioButton(tr("Up"), box); _chromaticUp->setChecked(true);
    connect(_chromaticUp, SIGNAL(toggled(bool)), this, SLOT(updateIntervalLabel()));
    _chromaticDown = new QRadioButton(tr("Down"), box);
    connect(_chromaticDown, SIGNAL(toggled(bool)), this, SLOT(updateIntervalLabel()));

    g->addWidget(new QLabel(tr("Preset:")), 0, 0); g->addWidget(_chromaticIntervalCombo, 0, 1, 1, 2);
    g->addWidget(new QLabel(tr("Shift:")), 1, 0); g->addWidget(_chromaticOctBox, 1, 1); g->addWidget(_chromaticMsBox, 1, 2);
    g->addWidget(_chromaticUp, 2, 1); g->addWidget(_chromaticDown, 2, 2);
    layout->addWidget(box);
    box->setMinimumHeight(160);

    QLabel *info = new QLabel(tr("Moves every note by the exact same distance. Preserves the shape of chords and melodies."), w);
    info->setWordWrap(true);
    info->setStyleSheet("font-size: 11px;");
    layout->addWidget(info);
    layout->addStretch();
    return w;
}

QWidget* TransposeDialog::createKeyTab() {
    QWidget *w = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    QGroupBox *box = new QGroupBox(tr("Key-to-Key Shift"), w);
    QGridLayout *g = new QGridLayout(box);
    _fromKeyRoot = new QComboBox(box); for (int i = 0; i < 12; ++i) _fromKeyRoot->addItem(ROOT_NOTES[i]);
    _fromKeyMode = new QComboBox(box);
    _toKeyRoot = new QComboBox(box); for (int i = 0; i < 12; ++i) _toKeyRoot->addItem(ROOT_NOTES[i]);
    _toKeyMode = new QComboBox(box);
    foreach(const ScaleDef& s, getScales()) {
        _fromKeyMode->addItem(s.name);
        _toKeyMode->addItem(s.name);
    }
    _keyDirection = new QComboBox(box);
    _keyDirection->addItem(tr("Up"), 0); _keyDirection->addItem(tr("Down"), 1); _keyDirection->addItem(tr("Shortest"), 2);
    _keyDirection->setCurrentIndex(2);
    
    connect(_fromKeyRoot, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_fromKeyMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_toKeyRoot, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_toKeyMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_keyDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));

    QPushButton *detectKeyBtn = new QPushButton(tr("Detect Best Scale"), box);
    connect(detectKeyBtn, &QPushButton::clicked, [this](){
        int r, t; detectScale(r, t);
        _fromKeyRoot->setCurrentIndex(r); _fromKeyMode->setCurrentIndex(t);
        updateIntervalLabel();
    });
    
    g->addWidget(new QLabel(tr("Original Key:")), 0, 0); g->addWidget(_fromKeyRoot, 0, 1); g->addWidget(_fromKeyMode, 0, 2);
    g->addWidget(detectKeyBtn, 1, 1, 1, 2);
    g->addWidget(new QLabel(tr("Target Key:")), 2, 0); g->addWidget(_toKeyRoot, 2, 1); g->addWidget(_toKeyMode, 2, 2);
    g->addWidget(new QLabel(tr("Direction:")), 3, 0); g->addWidget(_keyDirection, 3, 1, 1, 2);
    layout->addWidget(box);
    box->setMinimumHeight(160);

    QLabel *info = new QLabel(tr("Changes the key or mode. E.g., C Major to G Major preserves exact melody shape (chromatic), while C Major to C Minor applies Modal Interchange."), w);
    info->setWordWrap(true);
    info->setStyleSheet("font-size: 11px;");
    layout->addWidget(info);
    layout->addStretch();
    
    if (!_toTranspose.isEmpty()) {
        int r, t; detectScale(r, t);
        _fromKeyRoot->setCurrentIndex(r); _fromKeyMode->setCurrentIndex(t);
        _toKeyRoot->setCurrentIndex(r); _toKeyMode->setCurrentIndex(t);
    }
    return w;
}

QWidget* TransposeDialog::createScaleTab() {
    QWidget *w = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    QGroupBox *scaleGroup = new QGroupBox(tr("Diatonic Shift"), w);
    QGridLayout *scaleLayout = new QGridLayout(scaleGroup);
    _diatonicScaleRoot = new QComboBox(scaleGroup); for (int i = 0; i < 12; ++i) _diatonicScaleRoot->addItem(ROOT_NOTES[i]);
    _diatonicScaleType = new QComboBox(scaleGroup);
    foreach(const ScaleDef& s, getScales()) _diatonicScaleType->addItem(s.name);
    
    scaleLayout->addWidget(new QLabel(tr("Key:")), 0, 0); scaleLayout->addWidget(_diatonicScaleRoot, 0, 1); scaleLayout->addWidget(_diatonicScaleType, 0, 2);
    
    QPushButton *detectBtn = new QPushButton(tr("Detect Best Scale"), scaleGroup);
    connect(detectBtn, &QPushButton::clicked, [this](){
        int r, t; detectScale(r, t);
        _diatonicScaleRoot->setCurrentIndex(r); _diatonicScaleType->setCurrentIndex(t);
        updateIntervalLabel();
    });
    scaleLayout->addWidget(detectBtn, 1, 1, 1, 2);

    _diatonicSteps = new QSpinBox(scaleGroup); _diatonicSteps->setRange(0, 100); _diatonicSteps->setValue(1);
    _diatonicUp = new QRadioButton(tr("Up"), scaleGroup); _diatonicUp->setChecked(true);
    _diatonicDown = new QRadioButton(tr("Down"), scaleGroup);
    
    connect(_diatonicScaleRoot, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_diatonicScaleType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_diatonicSteps, SIGNAL(valueChanged(int)), this, SLOT(updateIntervalLabel()));
    connect(_diatonicUp, SIGNAL(toggled(bool)), this, SLOT(updateIntervalLabel()));
    connect(_diatonicDown, SIGNAL(toggled(bool)), this, SLOT(updateIntervalLabel()));

    scaleLayout->addWidget(new QLabel(tr("Shift:")), 2, 0); scaleLayout->addWidget(_diatonicSteps, 2, 1);
    scaleLayout->addWidget(new QLabel(tr("Scale Degree(s)")), 2, 2);
    QHBoxLayout *ud = new QHBoxLayout(); ud->addWidget(_diatonicUp); ud->addWidget(_diatonicDown);
    scaleLayout->addLayout(ud, 3, 1, 1, 2);
    layout->addWidget(scaleGroup);
    scaleGroup->setMinimumHeight(160);

    QLabel *info = new QLabel(tr("Forces notes to stay within the selected key. Useful for melodies and harmonies. Setting Shift to 0 will snap all notes to the nearest scale degree without shifting them."), w);
    info->setWordWrap(true);
    info->setStyleSheet("font-size: 11px;");
    layout->addWidget(info);
    layout->addStretch();
    
    if (!_toTranspose.isEmpty()) {
        int r, t; detectScale(r, t);
        _diatonicScaleRoot->setCurrentIndex(r);
        _diatonicScaleType->setCurrentIndex(t);
    }
    
    return w;
}

void TransposeDialog::onIntervalComboChanged(int index) {
    if (index > 0) {
        _chromaticOctBox->blockSignals(true); _chromaticMsBox->blockSignals(true);
        _chromaticOctBox->setValue(index / 12); _chromaticMsBox->setValue(index % 12);
        _chromaticOctBox->blockSignals(false); _chromaticMsBox->blockSignals(false);
        updateIntervalLabel();
    }
}

void TransposeDialog::updateIntervalLabel() {
    if (!_excludeDrums) return;
    int modeIdx = _modeSelector->currentIndex();
    int minN = 127, maxN = 0;
    int validCount = 0;
    foreach(NoteOnEvent* e, _toTranspose) {
        if (_excludeDrums->isChecked() && e->channel() == 9) continue;
        validCount++;
        if (e->note() < minN) minN = e->note();
        if (e->note() > maxN) maxN = e->note();
    }
    if (validCount == 0) {
        _chromaticIntervalInfo->setText(tr("<b>Selection:</b> No notes to transpose (or all excluded)"));
        return;
    }
    
    auto noteName = [](int n) { return QString("%1%2").arg(ROOT_NOTES[qBound(0, n, 127) % 12]).arg(qBound(0, n, 127) / 12 - 1); };
    QString baseInfo = tr("<b>Selection:</b> %1 notes (%2 to %3)").arg(validCount).arg(noteName(minN)).arg(noteName(maxN));
    QString chord = identifyChord();
    if (!chord.isEmpty()) baseInfo += tr("<br><b>Detected Chord:</b> %1").arg(chord);

    int shift = 0;
    if (modeIdx == 0) { // Simple
        shift = _chromaticOctBox->value() * 12 + _chromaticMsBox->value();
        if (_chromaticDown->isChecked()) shift *= -1;
        _chromaticIntervalInfo->setText(baseInfo + tr("<br><b>Result:</b> %1 to %2 (%3 semitones)").arg(noteName(minN + shift)).arg(noteName(maxN + shift)).arg(shift > 0 ? tr("+%1").arg(shift) : QString::number(shift)));
    } else if (modeIdx == 1) { // Key
        int fromRoot = _fromKeyRoot->currentIndex(), toRoot = _toKeyRoot->currentIndex();
        int fromType = _fromKeyMode->currentIndex(), toType = _toKeyMode->currentIndex();
        int newMin = 127, newMax = 0;
        foreach(NoteOnEvent* e, _toTranspose) {
            if (_excludeDrums->isChecked() && e->channel() == 9) continue;
            int n = performModalInterchange(e->note(), fromRoot, fromType, toRoot, toType);
            if (n < newMin) newMin = n;
            if (n > newMax) newMax = n;
        }
        QString shiftText = (fromType == toType) ? tr("Chromatic Shift") : tr("Modal Interchange");
        _chromaticIntervalInfo->setText(baseInfo + tr("<br><b>Result:</b> %1 to %2 (%3)").arg(noteName(newMin)).arg(noteName(newMax)).arg(shiftText));
    } else if (modeIdx == 2) { // Diatonic
        int root = _diatonicScaleRoot->currentIndex(), type = _diatonicScaleType->currentIndex();
        int steps = _diatonicSteps->value(); if (_diatonicDown->isChecked()) steps *= -1;
        int newMin = 127, newMax = 0;
        foreach(NoteOnEvent* e, _toTranspose) {
            if (_excludeDrums->isChecked() && e->channel() == 9) continue;
            int n = performDiatonicTranspose(e->note(), steps, root, type);
            if (n < newMin) newMin = n;
            if (n > newMax) newMax = n;
        }
        QString stepStr = steps == 0 ? tr("Snap to Scale") : (steps > 0 ? tr("+%1 steps").arg(steps) : tr("%1 steps").arg(steps));
        _chromaticIntervalInfo->setText(baseInfo + tr("<br><b>Result:</b> %1 to %2 (Diatonic %3)").arg(noteName(newMin)).arg(noteName(newMax)).arg(stepStr));
    }
}

int TransposeDialog::performModalInterchange(int note, int srcRoot, int srcType, int tgtRoot, int tgtType) {
    QList<ScaleDef> scales = getScales();
    QList<int> srcPat = scales[srcType].pattern;
    QList<int> tgtPat = scales[tgtType].pattern;
    
    int pitchClass = note % 12;
    int octave = note / 12;
    int srcRootOctave = octave;
    if (pitchClass < srcRoot) srcRootOctave--;
    
    int relativePitch = (pitchClass - srcRoot + 12) % 12;
    
    int currentIdx = -1;
    for (int i = 0; i < srcPat.size(); ++i) {
        if (srcPat[i] == relativePitch) { currentIdx = i; break; }
    }
    if (currentIdx == -1) {
        for (int i = srcPat.size() - 1; i >= 0; --i) {
            if (srcPat[i] < relativePitch) { currentIdx = i; break; }
        }
        if (currentIdx == -1) currentIdx = srcPat.size() - 1;
    }
    
    int accidental = relativePitch - srcPat[currentIdx];
    int tgtIdx = currentIdx;
    if (tgtIdx >= tgtPat.size()) tgtIdx = tgtPat.size() - 1;
    
    int tgtRelativePitch = tgtPat[tgtIdx] + accidental;
    
    int dir = _keyDirection->currentIndex();
    int up = (tgtRoot - srcRoot + 12) % 12, down = up - 12;
    int shiftDir = (dir == 1) ? down : (dir == 2 ? (up <= 6 ? up : down) : up);
    
    int srcRootNote = srcRootOctave * 12 + srcRoot;
    int tgtRootNote = srcRootNote + shiftDir;
    
    int newNote = tgtRootNote + tgtRelativePitch;
    return newNote;
}

int TransposeDialog::performDiatonicTranspose(int note, int steps, int root, int scaleType) {
    QList<ScaleDef> scales = getScales();
    if (scaleType >= scales.size()) return note;
    
    QList<int> pattern = scales[scaleType].pattern;
    int scaleSize = pattern.size();
    
    int pitchClass = note % 12;
    int octave = note / 12;
    
    int rootOctave = octave;
    if (pitchClass < root) {
        rootOctave--;
    }
    int rootNote = rootOctave * 12 + root;
    
    int relativePitch = (pitchClass - root + 12) % 12;
    
    int currentIdx = -1;
    for (int i = 0; i < scaleSize; ++i) {
        if (pattern[i] == relativePitch) {
            currentIdx = i;
            break;
        }
    }
    if (currentIdx == -1) {
        for (int i = scaleSize - 1; i >= 0; --i) {
            if (pattern[i] < relativePitch) {
                currentIdx = i;
                break;
            }
        }
        if (currentIdx == -1) currentIdx = scaleSize - 1;
    }
    
    int newIdx = currentIdx + steps;
    int octaves = (newIdx >= 0) ? (newIdx / scaleSize) : ((newIdx - scaleSize + 1) / scaleSize);
    int normIdx = newIdx - octaves * scaleSize;
    
    int newNote = rootNote + octaves * 12 + pattern[normIdx];
    return newNote;
}

void TransposeDialog::accept() {
    int modeIdx = _modeSelector->currentIndex();
    QString actionName = tr("Transpose Selection");
    _file->protocol()->startNewAction(actionName, new QImage(":/run_environment/graphics/tool/transpose.png"));

    if (modeIdx == 0) { // Simple Chromatic
        int shift = _chromaticOctBox->value() * 12 + _chromaticMsBox->value();
        if (_chromaticDown->isChecked()) shift *= -1;
        if (_protectRange && !_toTranspose.isEmpty()) {
            int minN = 127, maxN = 0;
            foreach(NoteOnEvent* e, _toTranspose) {
                if (_excludeDrums->isChecked() && e->channel() == 9) continue;
                if (e->note() < minN) minN = e->note(); 
                if (e->note() > maxN) maxN = e->note(); 
            }
            if (minN <= 127) { // Valid notes found
                if (minN + shift < 0) shift = -minN;
                if (maxN + shift > 127) shift = 127 - maxN;
            }
        }
        foreach(NoteOnEvent* e, _toTranspose) {
            if (_excludeDrums->isChecked() && e->channel() == 9) continue;
            e->setNote(qBound(0, e->note() + shift, 127));
        }
    } else if (modeIdx == 1) { // Key-to-Key (Modal Interchange)
        int fromRoot = _fromKeyRoot->currentIndex(), toRoot = _toKeyRoot->currentIndex();
        int fromType = _fromKeyMode->currentIndex(), toType = _toKeyMode->currentIndex();
        foreach(NoteOnEvent* e, _toTranspose) {
            if (_excludeDrums->isChecked() && e->channel() == 9) continue;
            int n = performModalInterchange(e->note(), fromRoot, fromType, toRoot, toType);
            e->setNote(qBound(0, n, 127));
        }
    } else if (modeIdx == 2) { // Scale-Aware Diatonic
        int root = _diatonicScaleRoot->currentIndex(), type = _diatonicScaleType->currentIndex();
        int steps = _diatonicSteps->value(); if (_diatonicDown->isChecked()) steps *= -1;
        
        if (_protectRange && !_toTranspose.isEmpty()) {
            int effectiveSteps = steps;
            while (effectiveSteps != 0) {
                int maxN = 0, minN = 127;
                foreach(NoteOnEvent* e, _toTranspose) {
                    if (_excludeDrums->isChecked() && e->channel() == 9) continue;
                    int n = performDiatonicTranspose(e->note(), effectiveSteps, root, type);
                    if (n > maxN) maxN = n;
                    if (n < minN) minN = n;
                }
                if (minN >= 0 && maxN <= 127) break;
                effectiveSteps += (steps > 0 ? -1 : 1);
            }
            steps = effectiveSteps;
        }
        
        foreach(NoteOnEvent* e, _toTranspose) {
            if (_excludeDrums->isChecked() && e->channel() == 9) continue;
            int n = performDiatonicTranspose(e->note(), steps, root, type);
            e->setNote(qBound(0, n, 127));
        }
    }
    _file->protocol()->endAction(); hide();
}
