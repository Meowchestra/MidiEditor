#include "ApplicationSettingsWidget.h"
#include "Appearance.h"

ApplicationSettingsWidget::ApplicationSettingsWidget(QWidget* parent) : QWidget(parent) {
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // High DPI Scaling section
    QLabel* scalingHeader = new QLabel("<b>High DPI Scaling</b>");
    layout->addWidget(scalingHeader, 0, 0, 1, 2);

    layout->addWidget(new QLabel("Ignore System Scaling"), 1, 0, 1, 1);
    QCheckBox *ignoreScaling = new QCheckBox(this);
    ignoreScaling->setChecked(Appearance::ignoreSystemScaling());
    ignoreScaling->setToolTip("Completely disable high DPI scaling (like older versions).\nProvides smallest UI but may be hard to read on high DPI displays.\nRequires application restart to take effect.");
    connect(ignoreScaling, SIGNAL(toggled(bool)), this, SLOT(ignoreScalingChanged(bool)));
    layout->addWidget(ignoreScaling, 1, 1, 1, 1);

    layout->addWidget(new QLabel("Use Rounded Scaling Behavior"), 2, 0, 1, 1);
    QCheckBox *roundedScaling = new QCheckBox(this);
    roundedScaling->setChecked(Appearance::useRoundedScaling());
    roundedScaling->setToolTip("Use integer scaling (100%, 200%, 300%) instead of fractional scaling (125%, 150%).\nQt5 used rounded scaling which provides sharper text rendering.\nQt6 uses fractional scaling which can appear blurry on some displays.\nRequires application restart to take effect.");
    connect(roundedScaling, SIGNAL(toggled(bool)), this, SLOT(roundedScalingChanged(bool)));
    layout->addWidget(roundedScaling, 2, 1, 1, 1);

    // Add some spacing
    layout->setRowMinimumHeight(3, 20);

    // Future application settings can be added here
    // For example: auto-save, backup settings, etc.

    setLayout(layout);
}

void ApplicationSettingsWidget::ignoreScalingChanged(bool ignore) {
    Appearance::setIgnoreSystemScaling(ignore);
}

void ApplicationSettingsWidget::roundedScalingChanged(bool useRounded) {
    Appearance::setUseRoundedScaling(useRounded);
}
