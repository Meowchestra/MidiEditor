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

#ifndef APPEARANCESETTINGSWIDGET_H_
#define APPEARANCESETTINGSWIDGET_H_

// Project includes
#include "SettingsWidget.h"
#include "ColoredWidget.h"

// Qt includes
#include <QList>

// Forward declarations
class QCheckBox;

/**
 * \class NamedColorWidgetItem
 *
 * \brief Individual color picker item for channels and tracks.
 *
 * NamedColorWidgetItem represents a single color selection widget that
 * displays a color swatch with a name/number. It provides click-to-edit
 * functionality for customizing colors.
 */
class NamedColorWidgetItem : public QWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new NamedColorWidgetItem.
     * \param number The channel or track number
     * \param name Display name for the item
     * \param color Initial color value
     * \param parent The parent widget
     */
    NamedColorWidgetItem(int number, QString name, QColor color, QWidget *parent = 0);

    /**
     * \brief Gets the item number.
     * \return The channel or track number
     */
    int number();

signals:
    /**
     * \brief Emitted when the color is changed.
     * \param number The item number
     * \param c The new color
     */
    void colorChanged(int number, QColor c);

public slots:
    /**
     * \brief Handles color change from color picker.
     * \param color The new color
     */
    void colorChanged(QColor color);

protected:
    /**
     * \brief Handles mouse press to open color picker.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event);

private:
    /** \brief The item number (channel or track) */
    int _number;

    /** \brief Current color value */
    QColor color;

    /** \brief Color display widget */
    ColoredWidget *colored;
};

/**
 * \class AppearanceSettingsWidget
 *
 * \brief Settings widget for appearance and visual customization.
 *
 * AppearanceSettingsWidget provides a comprehensive interface for customizing
 * the visual appearance of the MIDI editor, including:
 *
 * - **Color customization**: Channel and track color selection
 * - **Visual effects**: Opacity, strip styles, range lines
 * - **UI styling**: Application style and theme selection
 * - **Color management**: Reset and refresh color options
 *
 * The widget integrates with the Appearance class to provide persistent
 * visual customization options.
 */
class AppearanceSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new AppearanceSettingsWidget.
     * \param parent The parent widget
     */
    AppearanceSettingsWidget(QWidget *parent = 0);

public slots:
    /**
     * \brief Handles channel color changes.
     * \param channel The channel number
     * \param c The new color
     */
    void channelColorChanged(int channel, QColor c);

    /**
     * \brief Handles track color changes.
     * \param track The track number
     * \param c The new color
     */
    void trackColorChanged(int track, QColor c);

    /**
     * \brief Resets all colors to defaults.
     */
    void resetColors();

    /**
     * \brief Refreshes colors when they're auto-updated.
     */
    void refreshColors();

    /**
     * \brief Handles opacity changes.
     * \param opacity The new opacity value
     */
    void opacityChanged(int opacity);

    /**
     * \brief Handles strip style changes.
     * \param strip The new strip style
     */
    void stripStyleChanged(int strip);

    /**
     * \brief Handles range lines setting changes.
     * \param enabled True to enable range lines
     */
    void rangeLinesChanged(bool enabled);

    /**
     * \brief Handles application style changes.
     * \param style The new style name
     */
    void styleChanged(const QString &style);

private:
    /** \brief List of channel color items */
    QList<NamedColorWidgetItem *> *_channelItems;

    /** \brief List of track color items */
    QList<NamedColorWidgetItem *> *_trackItems;
};

#endif // APPEARANCESETTINGSWIDGET_H_
