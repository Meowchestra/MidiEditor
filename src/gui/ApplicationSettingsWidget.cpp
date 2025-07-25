#include "ApplicationSettingsWidget.h"
#include "Appearance.h"
#include <QIcon>

ApplicationSettingsWidget::ApplicationSettingsWidget(QWidget* parent) : SettingsWidget("System", parent) {
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(10, 5, 10, 10);
    layout->setVerticalSpacing(3);
    layout->setHorizontalSpacing(10);

    // High DPI Scaling section - at the very top
    QLabel* scalingHeader = new QLabel("<b>High DPI Scaling</b>");
    layout->addWidget(scalingHeader, 0, 0, 1, 2);

    // Ignore System Scaling
    QLabel* ignoreLabel = new QLabel("Ignore System Scaling:");
    ignoreLabel->setWordWrap(true);
    layout->addWidget(ignoreLabel, 1, 0, 1, 1);

    QCheckBox *ignoreScaling = new QCheckBox(this);
    ignoreScaling->setChecked(Appearance::ignoreSystemScaling());
    layout->addWidget(ignoreScaling, 1, 1, 1, 1);

    QLabel* ignoreDesc = new QLabel("Completely disable high DPI scaling (like older versions). Provides smallest UI but may be hard to read on high DPI displays. Requires application restart to take effect.");
    ignoreDesc->setWordWrap(true);
    ignoreDesc->setStyleSheet("color: gray; font-size: 10px; margin-left: 10px;");
    layout->addWidget(ignoreDesc, 2, 0, 1, 2);
    connect(ignoreScaling, SIGNAL(toggled(bool)), this, SLOT(ignoreScalingChanged(bool)));

    // Use Rounded Scaling
    QLabel* roundedLabel = new QLabel("Use Rounded Scaling Behavior:");
    roundedLabel->setWordWrap(true);
    layout->addWidget(roundedLabel, 3, 0, 1, 1);

    QCheckBox *roundedScaling = new QCheckBox(this);
    roundedScaling->setChecked(Appearance::useRoundedScaling());
    layout->addWidget(roundedScaling, 3, 1, 1, 1);

    QLabel* roundedDesc = new QLabel("Use integer scaling (100%, 200%, 300%) instead of fractional scaling (125%, 150%). Qt5 used rounded scaling which provides sharper text rendering. Qt6 uses fractional scaling which can appear blurry on some displays. Requires application restart to take effect.");
    roundedDesc->setWordWrap(true);
    roundedDesc->setStyleSheet("color: gray; font-size: 10px; margin-left: 10px;");
    layout->addWidget(roundedDesc, 4, 0, 1, 2);
    connect(roundedScaling, SIGNAL(toggled(bool)), this, SLOT(roundedScalingChanged(bool)));

    // Add stretch to push everything to the top
    layout->setRowStretch(5, 1);

    setLayout(layout);
}

void ApplicationSettingsWidget::ignoreScalingChanged(bool ignoreScaling) {
    Appearance::setIgnoreSystemScaling(ignoreScaling);
}

void ApplicationSettingsWidget::roundedScalingChanged(bool useRounded) {
    Appearance::setUseRoundedScaling(useRounded);
}