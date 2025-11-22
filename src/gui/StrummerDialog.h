/*
 * MidiEditor
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STRUMMERDIALOG_H_
#define STRUMMERDIALOG_H_

#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

/**
 * \class StrummerDialog
 *
 * \brief Dialog for configuring the Strummer tool options.
 */
class StrummerDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new StrummerDialog.
     * \param parent Parent widget
     */
    explicit StrummerDialog(QWidget *parent = nullptr);

    int getStartStrength() const;
    double getStartTension() const;
    int getEndStrength() const;
    double getEndTension() const;
    int getVelocityStrength() const;
    double getVelocityTension() const;
    bool getPreserveEnd() const;
    bool getAlternateDirection() const;
    bool getUseStepStrength() const;
    bool getIgnoreTrack() const;

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    QSpinBox *_startStrengthSpin;
    QDoubleSpinBox *_startTensionSpin;
    QSpinBox *_endStrengthSpin;
    QDoubleSpinBox *_endTensionSpin;
    QSpinBox *_velocityStrengthSpin;
    QDoubleSpinBox *_velocityTensionSpin;
    QCheckBox *_preserveEndCheck;
    QCheckBox *_alternateDirectionCheck;
    QCheckBox *_useStepStrengthCheck;
    QCheckBox *_ignoreTrackCheck;
    QPushButton *_okButton;
    QPushButton *_cancelButton;

    void setupUI();
    void setupConnections();
};

#endif // STRUMMERDIALOG_H_
