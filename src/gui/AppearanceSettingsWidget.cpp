#include "AppearanceSettingsWidget.h"

#define ROW_HEIGHT 45

#include <QGridLayout>
#include <QListWidget>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QList>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>

#include "Appearance.h"

AppearanceSettingsWidget::AppearanceSettingsWidget(QWidget* parent)
    : SettingsWidget("Appearance", parent)
{
    QGridLayout* layout = new QGridLayout(this);
    setLayout(layout);

    // Set minimum size to prevent overlapping elements
    setMinimumSize(400, 700);

    _channelItems = new QList<NamedColorWidgetItem*>();
    _trackItems = new QList<NamedColorWidgetItem*>();
    layout->addWidget(new QLabel("Channel Colors"), 0, 0, 1, 2);
    QListWidget *channelList = new QListWidget(this);
    channelList->setSelectionMode(QAbstractItemView::NoSelection);
    channelList->setStyleSheet("QListWidget::item { border-bottom: 1px solid lightGray; }");
    layout->addWidget(channelList, 1, 0, 1, 2);
    for (int i = 0; i < 17; i++) {
        QString name = "Channel " + QString::number(i);
        if (i == 16) {
            name = "General Events (affecting all channels)";
        }
        QColor* channelColor = Appearance::channelColor(i);
        QColor safeColor = channelColor ? *channelColor : QColor(100, 100, 100);
        NamedColorWidgetItem *channelItem = new NamedColorWidgetItem(i, name,
                                                                     safeColor, this);
        QListWidgetItem* item = new QListWidgetItem();
        item->setSizeHint(QSize(0, ROW_HEIGHT));
        channelList->addItem(item);
        channelList->setItemWidget(item, channelItem);
        _channelItems->append(channelItem);
        connect(channelItem, SIGNAL(colorChanged(int,QColor)), this, SLOT(channelColorChanged(int,QColor)));
    }
    channelList->setFixedHeight(ROW_HEIGHT * 5);
    channelList->setMinimumHeight(ROW_HEIGHT * 5); // Prevent shrinking below this size

    layout->addWidget(new QLabel("Track Colors"), 2, 0, 1, 2);
    QListWidget *trackList = new QListWidget(this);
    trackList->setSelectionMode(QAbstractItemView::NoSelection);
    trackList->setStyleSheet("QListWidget::item { border-bottom: 1px solid lightGray; }");
    layout->addWidget(trackList, 3, 0, 1, 2);
    for (int i = 0; i < 16; i++) {
        QColor* trackColor = Appearance::trackColor(i);
        QColor safeColor = trackColor ? *trackColor : QColor(100, 100, 100);
        NamedColorWidgetItem *trackItem = new NamedColorWidgetItem(i, "Track " + QString::number(i),
                                                                     safeColor, this);
        QListWidgetItem* item = new QListWidgetItem();
        item->setSizeHint(QSize(0, ROW_HEIGHT));
        trackList->addItem(item);
        trackList->setItemWidget(item, trackItem);
        _trackItems->append(trackItem);
        connect(trackItem, SIGNAL(colorChanged(int,QColor)), this, SLOT(trackColorChanged(int,QColor)));
    }
    trackList->setFixedHeight(ROW_HEIGHT * 5);
    trackList->setMinimumHeight(ROW_HEIGHT * 5); // Prevent shrinking below this size
    QPushButton *resetButton = new QPushButton("Reset Colors", this);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetColors()));
    layout->addWidget(resetButton, 4, 1, 1, 1);


    layout->addWidget(new QLabel("Event Opacity"), 6, 0, 1, 1);
    QSlider *opacity = new QSlider(Qt::Horizontal, this);
    opacity->setMaximum(0);
    opacity->setMaximum(100);
    opacity->setValue(Appearance::opacity());
    connect(opacity, SIGNAL(valueChanged(int)), this, SLOT(opacityChanged(int)));
    layout->addWidget(opacity, 6, 1, 1, 1);

    layout->addWidget(new QLabel("Strip Style"),7,0,1,1);
    QComboBox *strip = new QComboBox(this);
    strip->addItems({
                        "Highlight between octaves",
                        "Highlight notes by keys",
                        "Highlight alternatively"
                    });
    strip->setCurrentIndex(Appearance::strip());
    connect(strip, SIGNAL(currentIndexChanged(int)), this, SLOT(stripStyleChanged(int)));
    layout->addWidget(strip,7,1,1,1);

    layout->addWidget(new QLabel("Show C3/C6 Range Lines"), 8, 0, 1, 1);
    QCheckBox *rangeLines = new QCheckBox(this);
    rangeLines->setChecked(Appearance::showRangeLines());
    connect(rangeLines, SIGNAL(toggled(bool)), this, SLOT(rangeLinesChanged(bool)));
    layout->addWidget(rangeLines, 8, 1, 1, 1);

    // UI Styling options
    layout->addWidget(new QLabel("Application Style"), 9, 0, 1, 1);
    QComboBox *styleCombo = new QComboBox(this);
    QStringList availableStyles = Appearance::availableStyles();
    styleCombo->addItems(availableStyles);
    int currentIndex = availableStyles.indexOf(Appearance::applicationStyle());
    if (currentIndex >= 0) {
        styleCombo->setCurrentIndex(currentIndex);
    }
    connect(styleCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(styleChanged(QString)));
    layout->addWidget(styleCombo, 9, 1, 1, 1);

    layout->addWidget(new QLabel("Toolbar Icon Size"), 10, 0, 1, 1);
    QSpinBox *iconSize = new QSpinBox(this);
    iconSize->setMinimum(16);
    iconSize->setMaximum(32);
    iconSize->setValue(Appearance::toolbarIconSize());
    connect(iconSize, SIGNAL(valueChanged(int)), this, SLOT(iconSizeChanged(int)));
    layout->addWidget(iconSize, 10, 1, 1, 1);
}

void AppearanceSettingsWidget::channelColorChanged(int channel, QColor c){
    Appearance::setChannelColor(channel, c);
}

void AppearanceSettingsWidget::trackColorChanged(int track, QColor c) {
    Appearance::setTrackColor(track, c);
}

void AppearanceSettingsWidget::resetColors() {
    Appearance::reset();
    refreshColors();
}

void AppearanceSettingsWidget::refreshColors() {
    // Refresh all color widgets to show current colors
    foreach (NamedColorWidgetItem* item, *_trackItems) {
        QColor* trackColor = Appearance::trackColor(item->number());
        if (trackColor) {
            item->colorChanged(*trackColor);
        }
    }
    foreach (NamedColorWidgetItem* item, *_channelItems) {
        QColor* channelColor = Appearance::channelColor(item->number());
        if (channelColor) {
            item->colorChanged(*channelColor);
        }
    }
    update();
}


void AppearanceSettingsWidget::opacityChanged(int opacity) {
    Appearance::setOpacity(opacity);
    foreach (NamedColorWidgetItem* item, *_trackItems) {
        item->colorChanged(*Appearance::trackColor(item->number()));
    }
    foreach (NamedColorWidgetItem* item, *_channelItems) {
        item->colorChanged(*Appearance::channelColor(item->number()));
    }
    update();
}

void AppearanceSettingsWidget::stripStyleChanged(int strip){
    Appearance::setStrip(static_cast<Appearance::stripStyle>(strip));
    update();
}

void AppearanceSettingsWidget::rangeLinesChanged(bool enabled){
    Appearance::setShowRangeLines(enabled);
    update();
}

void AppearanceSettingsWidget::styleChanged(const QString& style){
    Appearance::setApplicationStyle(style);

    // Force immediate color refresh for all widgets
    Appearance::forceColorRefresh();
    refreshColors(); // Also refresh this widget's colors immediately
    update();
}

void AppearanceSettingsWidget::iconSizeChanged(int size){
    Appearance::setToolbarIconSize(size);
    update();
}

NamedColorWidgetItem::NamedColorWidgetItem(int number, QString name, QColor color, QWidget* parent) : QWidget(parent)
{
    this->_number = number;
    this->color = color;

    setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout(this);
    setLayout(layout);
    layout->setVerticalSpacing(1);

    colored = new ColoredWidget(color, this);
    colored->setFixedSize(ROW_HEIGHT-15, ROW_HEIGHT-15);
    layout->addWidget(colored, 0, 0, 1, 1);

    QLabel* text = new QLabel(name, this);
    text->setFixedHeight(15);
    layout->addWidget(text, 0, 1, 1, 1);
    setContentsMargins(5, 1, 5, 0);
    setFixedHeight(ROW_HEIGHT);
}

void NamedColorWidgetItem::mousePressEvent(QMouseEvent* event) {
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

int NamedColorWidgetItem::number(){
    return _number;
}
