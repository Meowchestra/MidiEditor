#include "FFXIVFixerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QDialogButtonBox>

FFXIVFixerDialog::FFXIVFixerDialog(const QJsonObject &analysis, QWidget *parent)
    : QDialog(parent), _analysis(analysis) {
    setWindowTitle(tr("Fix XIV Channels"));
    setMinimumWidth(480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Summary Section
    int trackCount = analysis["trackCount"].toInt();
    int ffxivTrackCount = analysis["ffxivTrackCount"].toInt();

    QString summaryHtml = QStringLiteral(
        "<h3>Analysis Complete</h3>"
        "<p>Detected <b>%1</b> tracks matching standard FFXIV instruments out of <b>%2</b> total tracks.</p>"
    ).arg(ffxivTrackCount).arg(trackCount);

    QJsonArray guitars = analysis["guitarVariants"].toArray();
    QJsonArray melodic = analysis["melodicTracks"].toArray();
    QJsonArray perc = analysis["percussionTracks"].toArray();

    summaryHtml += QStringLiteral("<p style='font-size: 11px; color: gray;'>");
    if (!melodic.isEmpty()) {
        summaryHtml += QStringLiteral("<b>Melodic:</b> ");
        for (int i = 0; i < melodic.size(); ++i) {
            summaryHtml += melodic[i].toString();
            if (i < melodic.size() - 1) summaryHtml += ", ";
        }
        summaryHtml += "<br>";
    }
    if (!guitars.isEmpty()) {
        summaryHtml += QStringLiteral("<b>Guitars:</b> ");
        for (int i = 0; i < guitars.size(); ++i) {
            summaryHtml += guitars[i].toString();
            if (i < guitars.size() - 1) summaryHtml += ", ";
        }
        summaryHtml += "<br>";
    }
    if (!perc.isEmpty()) {
        summaryHtml += QStringLiteral("<b>Percussion:</b> ");
        for (int i = 0; i < perc.size(); ++i) {
            summaryHtml += perc[i].toString();
            if (i < perc.size() - 1) summaryHtml += ", ";
        }
        summaryHtml += "<br>";
    }
    summaryHtml += "</p>";

    QLabel *summaryLabel = new QLabel(summaryHtml, this);
    summaryLabel->setTextFormat(Qt::RichText);
    summaryLabel->setWordWrap(true);
    mainLayout->addWidget(summaryLabel);

    // Mode Selection
    QGroupBox *modeGroup = new QGroupBox(tr("Processing Mode"), this);
    QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

    _rebuildRadio = new QRadioButton(tr("Rebuild: Reassign channels and program changes from scratch"), modeGroup);
    _preserveRadio = new QRadioButton(tr("Preserve: Keep existing channels, fix program changes only"), modeGroup);

    QString rebuildDesc = tr("Groups identical instruments into shared channels.");
    QString preserveDesc = tr("Preserves your hand-assigned channel layout.");

    QLabel *rDesc = new QLabel(rebuildDesc, modeGroup);
    rDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 20px; margin-bottom: 5px;");
    rDesc->setWordWrap(true);

    QLabel *pDesc = new QLabel(preserveDesc, modeGroup);
    pDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 20px; margin-bottom: 5px;");
    pDesc->setWordWrap(true);

    modeLayout->addWidget(_rebuildRadio);
    modeLayout->addWidget(rDesc);
    modeLayout->addWidget(_preserveRadio);
    modeLayout->addWidget(pDesc);

    mainLayout->addWidget(modeGroup);

    // Default to Rebuild mode
    _rebuildRadio->setChecked(true);

    // Options Section
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

    _normalizeVelocityCheck = new QCheckBox(tr("Normalize Velocity"), optionsGroup);
    _normalizeVelocityCheck->setChecked(true);
    _normalizeVelocityCheck->setToolTip(tr("Sets all note velocities to 100 for consistent volume."));
    optionsLayout->addWidget(_normalizeVelocityCheck);

    QLabel *cleanupLabel = new QLabel(tr("Cleanup unsupported events:"), optionsGroup);
    cleanupLabel->setTextFormat(Qt::RichText);
    optionsLayout->addWidget(cleanupLabel);

    QLabel *cleanupDesc = new QLabel(tr("FFXIV only uses Note & Program Change events."), optionsGroup);
    cleanupDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 4px; margin-bottom: 4px;");
    cleanupDesc->setWordWrap(true);
    optionsLayout->addWidget(cleanupDesc);

    _cleanupCCCheck = new QCheckBox(tr("Remove Control Change Events"), optionsGroup);
    _cleanupCCCheck->setChecked(true);
    optionsLayout->addWidget(_cleanupCCCheck);

    _cleanupKeyPressureCheck = new QCheckBox(tr("Remove Key Pressure Events"), optionsGroup);
    _cleanupKeyPressureCheck->setChecked(true);
    optionsLayout->addWidget(_cleanupKeyPressureCheck);

    _cleanupChannelPressureCheck = new QCheckBox(tr("Remove Channel Pressure Events"), optionsGroup);
    _cleanupChannelPressureCheck->setChecked(true);
    optionsLayout->addWidget(_cleanupChannelPressureCheck);

    _cleanupPitchBendCheck = new QCheckBox(tr("Remove Pitch Bend Events"), optionsGroup);
    _cleanupPitchBendCheck->setChecked(true);
    optionsLayout->addWidget(_cleanupPitchBendCheck);

    mainLayout->addWidget(optionsGroup);

    // Buttons
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
}

int FFXIVFixerDialog::selectedTier() const {
    if (_preserveRadio->isChecked()) return 3;
    if (_rebuildRadio->isChecked()) return 2;
    return 0;
}

bool FFXIVFixerDialog::cleanupControlChanges() const {
    return _cleanupCCCheck->isChecked();
}

bool FFXIVFixerDialog::cleanupKeyPressure() const {
    return _cleanupKeyPressureCheck->isChecked();
}

bool FFXIVFixerDialog::cleanupChannelPressure() const {
    return _cleanupChannelPressureCheck->isChecked();
}

bool FFXIVFixerDialog::cleanupPitchBend() const {
    return _cleanupPitchBendCheck->isChecked();
}

bool FFXIVFixerDialog::normalizeVelocity() const {
    return _normalizeVelocityCheck->isChecked();
}
