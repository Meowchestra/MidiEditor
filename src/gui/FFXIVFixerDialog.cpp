#include "FFXIVFixerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QGroupBox>
#include <QJsonArray>
#include <QDialogButtonBox>

FFXIVFixerDialog::FFXIVFixerDialog(const QJsonObject &analysis, QWidget *parent)
    : QDialog(parent), _analysis(analysis) {
    setWindowTitle(tr("Fix XIV Channels"));
    setMinimumWidth(450);

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

    QString rebuildDesc = tr("Groups identical instruments into shared channels. Best for MIDI files imported from scratch.");
    QString preserveDesc = tr("Preserves your hand-assigned channel layout. Required if you've done custom layering.");

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

    // Set Default based on analysis Auto-Tier
    int autoTier = analysis["autoDetectedTier"].toInt();
    if (autoTier == 3) {
        _preserveRadio->setChecked(true);
        QLabel *recLabel = new QLabel(tr("<b>Recommendation: Preserve</b><br>Electric guitar program changes were already found across multiple channels."), modeGroup);
        recLabel->setTextFormat(Qt::RichText);
        modeLayout->addWidget(recLabel);
    } else {
        _rebuildRadio->setChecked(true);
    }

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
