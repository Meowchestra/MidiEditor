#ifndef THEMEPALETTES_H_
#define THEMEPALETTES_H_

#include <QPalette>
#include <QString>

namespace ThemePalettes {

    /**
     * \brief Returns the palette for the Sakura theme
     */
    QPalette getSakuraPalette();

    /**
     * \brief Returns any specific stylesheet tweaks for Sakura
     */
    QString getSakuraStyleSheet();

    /**
     * \brief Returns the palette for the AMOLED theme (inspired by GTRONICK)
     */
    QPalette getAmoledPalette();

    /**
     * \brief Returns any specific stylesheet tweaks for AMOLED
     */
    QString getAmoledStyleSheet();

    /**
     * \brief Returns the palette for the Material Dark theme (inspired by GTRONICK)
     */
    QPalette getMaterialDarkPalette();

    /**
     * \brief Returns any specific stylesheet tweaks for Material Dark
     */
    QString getMaterialDarkStyleSheet();

    /**
     * \brief Returns the palette for the Nord theme
     */
    QPalette getNordPalette();

    /**
     * \brief Returns any specific stylesheet tweaks for Nord
     */
    QString getNordStyleSheet();

    /**
     * \brief Returns the palette for the Airy theme
     */
    QPalette getAiryPalette();

    /**
     * \brief Returns any specific stylesheet tweaks for Airy
     */
    QString getAiryStyleSheet();

}

#endif // THEMEPALETTES_H_
