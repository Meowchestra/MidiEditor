#include "AppearanceSettingsWidget.h"

#define ROW_HEIGHT 40

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QList>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QSettings>
#include <QApplication>
#include <QAbstractItemView>

#include "Appearance.h"

AppearanceSettingsWidget::AppearanceSettingsWidget(QWidget *parent)
    : SettingsWidget(tr("Appearance"), parent) {
    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    // Set minimum size and spacing to prevent overlapping elements
    setMinimumSize(450, 640);
    layout->setSpacing(4);
    layout->setContentsMargins(12, 8, 12, 4);

    _channelItems = new QList<NamedColorWidgetItem *>();
    _trackItems = new QList<NamedColorWidgetItem *>();
    
    QLabel *channelLabel = new QLabel(tr("Channel Colors"));
    channelLabel->setStyleSheet("margin-bottom: 2px;");
    layout->addWidget(channelLabel, 0, 0, 1, 2);
    channelLabel->setMinimumHeight(20);
    QListWidget *channelList = new QListWidget(this);
    channelList->setSelectionMode(QAbstractItemView::NoSelection);
    channelList->setStyleSheet("QListWidget::item { border-bottom: 1px solid lightGray; }");
    layout->addWidget(channelList, 1, 0, 1, 2);
    for (int i = 0; i < 17; i++) {
        QString name = tr("Channel %1").arg(i);
        if (i == 9) {
            name = tr("Channel 9 (Percussion)");
        } else if (i == 16) {
            name = tr("General Events (affecting all channels)");
        }
        QColor *channelColor = Appearance::channelColor(i);
        QColor safeColor = channelColor ? *channelColor : QColor(100, 100, 100);
        NamedColorWidgetItem *channelItem = new NamedColorWidgetItem(i, name,
                                                                     safeColor, this);
        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, ROW_HEIGHT));
        channelList->addItem(item);
        channelList->setItemWidget(item, channelItem);
        _channelItems->append(channelItem);
        connect(channelItem, SIGNAL(colorChanged(int,QColor)), this, SLOT(channelColorChanged(int,QColor)));
    }
    channelList->setFixedHeight(ROW_HEIGHT * 5);
    channelList->setMinimumHeight(ROW_HEIGHT * 5); // Prevent shrinking below this size

    QLabel *trackColorsLabel = new QLabel(tr("Track Colors"));
    trackColorsLabel->setStyleSheet("margin-top: 2px; margin-bottom: 2px;");
    trackColorsLabel->setMinimumHeight(20);
    layout->addWidget(trackColorsLabel, 2, 0, 1, 2);
    QListWidget *trackList = new QListWidget(this);
    trackList->setSelectionMode(QAbstractItemView::NoSelection);
    trackList->setStyleSheet("QListWidget::item { border-bottom: 1px solid lightGray; }");
    layout->addWidget(trackList, 3, 0, 1, 2);
    for (int i = 0; i < 16; i++) {
        QColor *trackColor = Appearance::trackColor(i);
        QColor safeColor = trackColor ? *trackColor : QColor(100, 100, 100);
        NamedColorWidgetItem *trackItem = new NamedColorWidgetItem(i, tr("Track %1").arg(i),
                                                                   safeColor, this);
        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, ROW_HEIGHT));
        trackList->addItem(item);
        trackList->setItemWidget(item, trackItem);
        _trackItems->append(trackItem);
        connect(trackItem, SIGNAL(colorChanged(int,QColor)), this, SLOT(trackColorChanged(int,QColor)));
    }
    trackList->setFixedHeight(ROW_HEIGHT * 5);
    trackList->setMinimumHeight(ROW_HEIGHT * 5); // Prevent shrinking below this size
    // Row 4: Opacity and Reset — wrapped in a widget with minimum height
    QWidget *topRowWidget = new QWidget(this);
    topRowWidget->setMinimumHeight(24);
    QHBoxLayout *topRow = new QHBoxLayout(topRowWidget);
    topRow->setContentsMargins(0, 0, 0, 0);
    QLabel *opacityLabel = new QLabel(tr("Event Opacity"));
    topRow->addWidget(opacityLabel, 0, Qt::AlignVCenter);
    QSlider *opacitySlider = new QSlider(Qt::Horizontal, this);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(Appearance::opacity());
    connect(opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(opacityChanged(int)));
    topRow->addWidget(opacitySlider, 1);
    topRow->addSpacing(20);
    
    QPushButton *resetButton = new QPushButton(tr("Reset Colors"), this);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetColors()));
    topRow->addWidget(resetButton, 0, Qt::AlignRight);
    
    // Add row constraint to prevent collapse
    layout->setRowMinimumHeight(4, 32);
    layout->addWidget(topRowWidget, 4, 0, 1, 2);

    // Row 5: Two columns
    QGridLayout *bottomGrid = new QGridLayout();
    bottomGrid->setContentsMargins(0, 2, 0, 0);
    layout->addLayout(bottomGrid, 5, 0, 1, 2);

    // Left Column: Visual Styles
    QGroupBox *styleGroup = new QGroupBox(tr("Visual Styles"), this);
    QGridLayout *styleLayout = new QGridLayout(styleGroup);
    styleLayout->setSpacing(6);
    styleLayout->setVerticalSpacing(10);
    styleLayout->setContentsMargins(8, 12, 8, 12);
    
    styleLayout->addWidget(new QLabel(tr("Application Style")), 0, 0);
    QComboBox *styleCombo = new QComboBox(this);
    QStringList availableStyles = Appearance::availableStyles();
    styleCombo->addItems(availableStyles);
    styleCombo->setCurrentText(Appearance::applicationStyle());
    connect(styleCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(styleChanged(QString)));
    styleLayout->addWidget(styleCombo, 0, 1);

    styleLayout->addWidget(new QLabel(tr("Color Preset")), 1, 0);
    QComboBox *presetCombo = new QComboBox(this);
    presetCombo->addItems({tr("Default"), tr("Pastel"), tr("Vibrant"), tr("Accessible"),
                           tr("FL Studio Classic"), tr("Ableton Live Muted"), tr("Logic Pro Distinct")});
    presetCombo->setCurrentIndex(static_cast<int>(Appearance::colorPreset()));
    connect(presetCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(colorPresetChanged(int)));
    styleLayout->addWidget(presetCombo, 1, 1);

    styleLayout->addWidget(new QLabel(tr("Strip Style")), 2, 0);
    QComboBox *strip = new QComboBox(this);
    strip->addItems({tr("Highlight by Octaves"), tr("Highlight by Scale"), tr("Highlight by Alternating")});
    strip->setCurrentIndex(Appearance::strip());
    connect(strip, SIGNAL(currentIndexChanged(int)), this, SLOT(stripStyleChanged(int)));
    styleLayout->addWidget(strip, 2, 1);

    styleLayout->addWidget(new QLabel(tr("Smooth Playback Scrolling")), 3, 0);
    QCheckBox *smoothScroll = new QCheckBox(this);
    smoothScroll->setChecked(Appearance::smoothPlaybackScrolling());
    connect(smoothScroll, SIGNAL(toggled(bool)), this, SLOT(smoothPlaybackScrollingChanged(bool)));
    styleLayout->addWidget(smoothScroll, 3, 1);
    
    bottomGrid->addWidget(styleGroup, 0, 0);

    // Right Column: Matrix & Marker Settings
    QGroupBox *markerGroup = new QGroupBox(tr("Matrix / Markers"), this);
    QGridLayout *markerLayout = new QGridLayout(markerGroup);
    markerLayout->setSpacing(6);
    markerLayout->setVerticalSpacing(10);
    markerLayout->setContentsMargins(8, 12, 8, 12);
    
    markerLayout->addWidget(new QLabel(tr("Show C3/C6 Range Lines")), 0, 0);
    QCheckBox *rangeLines = new QCheckBox(this);
    rangeLines->setChecked(Appearance::showRangeLines());
    connect(rangeLines, SIGNAL(toggled(bool)), this, SLOT(rangeLinesChanged(bool)));
    markerLayout->addWidget(rangeLines, 0, 1);

    markerLayout->addWidget(new QLabel(tr("Markers: Program Changes")), 1, 0);
    QCheckBox *pcMarkers = new QCheckBox(this);
    pcMarkers->setChecked(Appearance::showProgramChangeMarkers());
    connect(pcMarkers, SIGNAL(toggled(bool)), this, SLOT(programChangeMarkersChanged(bool)));
    markerLayout->addWidget(pcMarkers, 1, 1);

    markerLayout->addWidget(new QLabel(tr("Markers: Control Changes")), 2, 0);
    QCheckBox *ccMarkers = new QCheckBox(this);
    ccMarkers->setChecked(Appearance::showControlChangeMarkers());
    connect(ccMarkers, SIGNAL(toggled(bool)), this, SLOT(controlChangeMarkersChanged(bool)));
    markerLayout->addWidget(ccMarkers, 2, 1);

    markerLayout->addWidget(new QLabel(tr("Markers: Text Events")), 3, 0);
    QCheckBox *txtMarkers = new QCheckBox(this);
    txtMarkers->setChecked(Appearance::showTextEventMarkers());
    connect(txtMarkers, SIGNAL(toggled(bool)), this, SLOT(textEventMarkersChanged(bool)));
    markerLayout->addWidget(txtMarkers, 3, 1);
    bottomGrid->addWidget(markerGroup, 0, 1);

    
    // Push everything structurally up
    layout->setRowStretch(6, 1);
}

void AppearanceSettingsWidget::channelColorChanged(int channel, QColor c) {
    Appearance::setChannelColor(channel, c);
    emit appearanceChanged();
}

void AppearanceSettingsWidget::trackColorChanged(int track, QColor c) {
    Appearance::setTrackColor(track, c);
    emit appearanceChanged();
}

void AppearanceSettingsWidget::resetColors() {
    Appearance::reset();
    refreshColors();
    emit appearanceChanged();
}

void AppearanceSettingsWidget::refreshColors() {
    // Refresh all color widgets to show current colors
    foreach(NamedColorWidgetItem* item, *_trackItems) {
        QColor *trackColor = Appearance::trackColor(item->number());
        if (trackColor) {
            item->colorChanged(*trackColor);
        }
    }
    foreach(NamedColorWidgetItem* item, *_channelItems) {
        QColor *channelColor = Appearance::channelColor(item->number());
        if (channelColor) {
            item->colorChanged(*channelColor);
        }
    }
    update();
}

void AppearanceSettingsWidget::colorPresetChanged(int index) {
    Appearance::setColorPreset(static_cast<Appearance::ColorPreset>(index));
    resetColors();
}


void AppearanceSettingsWidget::opacityChanged(int opacity) {
    Appearance::setOpacity(opacity);
    foreach(NamedColorWidgetItem* item, *_trackItems) {
        item->colorChanged(*Appearance::trackColor(item->number()));
    }
    foreach(NamedColorWidgetItem* item, *_channelItems) {
        item->colorChanged(*Appearance::channelColor(item->number()));
    }
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::stripStyleChanged(int strip) {
    Appearance::setStrip(static_cast<Appearance::stripStyle>(strip));
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::rangeLinesChanged(bool enabled) {
    Appearance::setShowRangeLines(enabled);
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::programChangeMarkersChanged(bool enabled) {
    Appearance::setShowProgramChangeMarkers(enabled);
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::controlChangeMarkersChanged(bool enabled) {
    Appearance::setShowControlChangeMarkers(enabled);
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::textEventMarkersChanged(bool enabled) {
    Appearance::setShowTextEventMarkers(enabled);
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::smoothPlaybackScrollingChanged(bool enabled) {
    Appearance::setSmoothPlaybackScrolling(enabled);
    emit smoothScrollChanged(enabled);
    emit appearanceChanged();
    update();
}

void AppearanceSettingsWidget::styleChanged(const QString &style) {
    Appearance::setApplicationStyle(style);

    // Force immediate color refresh for all widgets
    Appearance::forceColorRefresh();
    refreshColors(); // Also refresh this widget's colors immediately
    update();
}

NamedColorWidgetItem::NamedColorWidgetItem(int number, QString name, QColor color, QWidget *parent) : QWidget(parent) {
    this->_number = number;
    this->color = color;

    setContentsMargins(0, 0, 0, 0);
    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);
    layout->setVerticalSpacing(1);

    colored = new ColoredWidget(color, this);
    colored->setFixedSize(ROW_HEIGHT - 15, ROW_HEIGHT - 15);
    layout->addWidget(colored, 0, 0, 1, 1);

    QLabel *text = new QLabel(name, this);
    text->setFixedHeight(18);
    layout->addWidget(text, 0, 1, 1, 1);
    setContentsMargins(5, 1, 5, 0);
    setFixedHeight(ROW_HEIGHT);
}

void NamedColorWidgetItem::mousePressEvent(QMouseEvent *event) {
    QColor newColor = QColorDialog::getColor(color, this);
    // Only apply the color if user didn't cancel (valid color returned)
    if (newColor.isValid()) {
        // Emit signal with raw color (Appearance::setChannelColor will apply opacity)
        emit colorChanged(_number, newColor);

        // But display the color with current opacity applied
        QColor displayColor = newColor;
        displayColor.setAlpha(Appearance::opacity() * 255 / 100);
        colored->setColor(displayColor);
        colored->update();
    }
    // If user canceled, do nothing - keep the original color
}

void NamedColorWidgetItem::colorChanged(QColor color) {
    // This slot is called when refreshing colors from the appearance system
    // The color already has opacity applied, so display it as-is
    colored->setColor(color);
    update();
    // Don't emit signal here - this is for display updates only
}

int NamedColorWidgetItem::number() {
    return _number;
}
