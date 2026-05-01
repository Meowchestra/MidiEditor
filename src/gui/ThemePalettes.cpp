#include "ThemePalettes.h"

namespace ThemePalettes {

QPalette getSakuraPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(255, 240, 245)); // LavenderBlush
    palette.setColor(QPalette::WindowText, QColor(90, 40, 60));
    palette.setColor(QPalette::Base, QColor(255, 250, 250)); // Snow
    palette.setColor(QPalette::AlternateBase, QColor(255, 230, 240));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 240, 245));
    palette.setColor(QPalette::ToolTipText, QColor(90, 40, 60));
    palette.setColor(QPalette::Text, QColor(60, 20, 40));
    palette.setColor(QPalette::Button, QColor(255, 220, 230));
    palette.setColor(QPalette::ButtonText, QColor(60, 20, 40));
    palette.setColor(QPalette::BrightText, QColor(150, 0, 50));
    palette.setColor(QPalette::Link, QColor(255, 105, 180)); // HotPink
    palette.setColor(QPalette::Highlight, QColor(255, 160, 180)); // Pink
    palette.setColor(QPalette::HighlightedText, QColor(60, 20, 40));
    palette.setColor(QPalette::PlaceholderText, QColor(120, 80, 100));
    palette.setColor(QPalette::Accent, QColor(255, 192, 203)); // Soft Pink for Windows 11 sliders/buttons
    return palette;
}

QString getSakuraStyleSheet() {
    return "QToolBar::separator { background-color: rgba(255, 182, 193, 150); width: 1px; height: 1px; margin: 2px; }";
}

QPalette getAmoledPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0, 0, 0));
    palette.setColor(QPalette::WindowText, QColor(169, 183, 198)); // #a9b7c6
    palette.setColor(QPalette::Base, QColor(0, 0, 0));
    palette.setColor(QPalette::AlternateBase, QColor(20, 20, 20));
    palette.setColor(QPalette::ToolTipBase, QColor(0, 0, 0));
    palette.setColor(QPalette::ToolTipText, QColor(230, 126, 34)); // #e67e22
    palette.setColor(QPalette::Text, QColor(169, 183, 198));
    palette.setColor(QPalette::Button, QColor(15, 15, 15));
    palette.setColor(QPalette::ButtonText, QColor(169, 183, 198));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(230, 126, 34)); // #e67e22
    palette.setColor(QPalette::Highlight, QColor(230, 126, 34)); // #e67e22
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Accent, QColor(230, 126, 34)); 
    return palette;
}

QString getAmoledStyleSheet() {
    return "QToolBar::separator { background-color: rgba(230, 126, 34, 150); width: 1px; height: 1px; margin: 2px; }";
}

QPalette getMaterialDarkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 29, 35)); // #1e1d23
    palette.setColor(QPalette::WindowText, QColor(169, 183, 198)); // #a9b7c6
    palette.setColor(QPalette::Base, QColor(40, 40, 40));
    palette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipBase, QColor(50, 50, 50));
    palette.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    palette.setColor(QPalette::Text, QColor(169, 183, 198));
    palette.setColor(QPalette::Button, QColor(40, 40, 40));
    palette.setColor(QPalette::ButtonText, QColor(169, 183, 198));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(4, 185, 127)); // #04b97f
    palette.setColor(QPalette::Highlight, QColor(4, 185, 127));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Accent, QColor(4, 185, 127)); 
    return palette;
}

QString getMaterialDarkStyleSheet() {
    return "QToolBar::separator { background-color: rgba(4, 185, 127, 150); width: 1px; height: 1px; margin: 2px; }";
}

QPalette getNordPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(46, 52, 64)); // nord0
    palette.setColor(QPalette::WindowText, QColor(216, 222, 233)); // nord4
    palette.setColor(QPalette::Base, QColor(59, 66, 82)); // nord1
    palette.setColor(QPalette::AlternateBase, QColor(67, 76, 94)); // nord2
    palette.setColor(QPalette::ToolTipBase, QColor(76, 86, 106)); // nord3
    palette.setColor(QPalette::ToolTipText, QColor(229, 233, 240)); // nord5
    palette.setColor(QPalette::Text, QColor(216, 222, 233)); // nord4
    palette.setColor(QPalette::Button, QColor(67, 76, 94)); // nord2
    palette.setColor(QPalette::ButtonText, QColor(216, 222, 233)); // nord4
    palette.setColor(QPalette::BrightText, QColor(191, 97, 106)); // nord11 (red)
    palette.setColor(QPalette::Link, QColor(136, 192, 208)); // nord8
    palette.setColor(QPalette::Highlight, QColor(129, 161, 193)); // nord9
    palette.setColor(QPalette::HighlightedText, QColor(46, 52, 64)); // nord0
    palette.setColor(QPalette::Accent, QColor(129, 161, 193));
    return palette;
}

QString getNordStyleSheet() {
    return "QToolBar::separator { background-color: rgba(76, 86, 106, 150); width: 1px; height: 1px; margin: 2px; }";
}

QPalette getAiryPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(250, 252, 255)); // Very light, slightly cool white
    palette.setColor(QPalette::WindowText, QColor(60, 70, 80)); // Soft dark blue-gray text
    palette.setColor(QPalette::Base, QColor(255, 255, 255)); // Pure white
    palette.setColor(QPalette::AlternateBase, QColor(245, 248, 252)); // Airy light blue-gray
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(60, 70, 80));
    palette.setColor(QPalette::Text, QColor(40, 50, 60));
    palette.setColor(QPalette::Button, QColor(240, 246, 252)); // Airy light blue button
    palette.setColor(QPalette::ButtonText, QColor(50, 60, 70));
    palette.setColor(QPalette::BrightText, QColor(255, 100, 100));
    palette.setColor(QPalette::Link, QColor(100, 160, 255));
    palette.setColor(QPalette::Highlight, QColor(210, 230, 255)); // Airy blue highlight
    palette.setColor(QPalette::HighlightedText, QColor(30, 40, 50));
    palette.setColor(QPalette::Accent, QColor(150, 200, 255));
    return palette;
}

QString getAiryStyleSheet() {
    return "QToolBar::separator { background-color: rgba(200, 210, 220, 150); width: 1px; height: 1px; margin: 2px; }";
}

} // namespace ThemePalettes
