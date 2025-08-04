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

#include <QApplication>
#include <QDir>
#include <QSurfaceFormat>
#include <QOpenGLContext>

#include "gui/MainWindow.h"
#include "gui/Appearance.h"
#include "midi/MidiInput.h"
#include "midi/MidiOutput.h"

#include <QFile>

#ifdef NO_CONSOLE_MODE
#include <tchar.h>
#include <windows.h>
std::string wstrtostr(const std::wstring &wstr) {
    std::string strTo;
    char *szTo = new char[wstr.length() + 1];
    szTo[wstr.size()] = '\0';
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, szTo, (int) wstr.length(),
                        NULL, NULL);
    strTo = szTo;
    delete[] szTo;
    return strTo;
}
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    int argc = 1;
    char *argv[] = {"", ""};
    std::string str;
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (NULL != szArglist && argc > 1) {
        str = wstrtostr(szArglist[1]);
        argv[1] = &str.at(0);
        argc = 2;
    }

#else
int main(int argc, char *argv[]) {
#endif

    // Load high DPI scaling settings before creating QApplication
    // These must be set before QApplication is created
    Appearance::loadEarlySettings();
    bool ignoreSystemScaling = Appearance::ignoreSystemScaling();
    bool useRoundedScaling = Appearance::useRoundedScaling();
    bool ignoreFontScaling = Appearance::ignoreFontScaling();
    int msaaSamples = Appearance::msaaSamples();
    bool enableVSync = Appearance::enableVSync();

    // Debug output to verify scaling settings
    qDebug() << "=== DPI Scaling Configuration ===";
    qDebug() << "Ignore system scaling:" << ignoreSystemScaling;
    qDebug() << "Ignore font scaling:" << ignoreFontScaling;
    qDebug() << "Use rounded scaling:" << useRoundedScaling;
    qDebug() << "MSAA samples:" << msaaSamples;
    qDebug() << "VSync enabled:" << enableVSync;

    // High DPI scaling is always enabled in Qt 6, so we only need to configure the scaling policy
    if (ignoreSystemScaling) {
        // For Qt 6, we need to be more aggressive to truly ignore system scaling
        qDebug() << "Setting aggressive scaling override to ignore system scaling";

        // Set multiple environment variables to force 1.0 scaling
        qputenv("QT_SCALE_FACTOR", "1.0");
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
        qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
        qputenv("QT_DEVICE_PIXEL_RATIO", "1.0");
        qputenv("QT_SCREEN_SCALE_FACTORS", "1.0");
    } else {
        if (useRoundedScaling) {
            // Use rounded scaling behavior for sharper rendering
            // Set rounding policy to Round instead of PassThrough (Qt6 default)
            qDebug() << "Setting rounded scaling policy";
            QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);

            // Enable high DPI scaling with rounding
            qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
            qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "Round");

            // Use integer-based DPI awareness
            qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
        } else {
            // Use Qt6 default behavior (PassThrough with fractional scaling)
            qDebug() << "Using Qt6 default PassThrough scaling policy";
            QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        }
    }

    // Handle font scaling separately from UI scaling
    if (ignoreFontScaling) {
        qDebug() << "Setting font scaling override to ignore font scaling";
        // Disable font DPI scaling to keep fonts at their original sizes
        qputenv("QT_FONT_DPI", "96"); // Force standard 96 DPI for fonts
        qputenv("QT_USE_PHYSICAL_DPI", "0"); // Don't use physical DPI for font sizing
    }

    QApplication a(argc, argv);

    // Additional font scaling control after QApplication creation
    if (ignoreFontScaling) {
        // Force the application to use 96 DPI for all font calculations
        QApplication::setAttribute(Qt::AA_Use96Dpi, true);
    }

    // Debug actual scaling factors after QApplication creation
    qDebug() << "=== Actual DPI Scaling Results ===";
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        qDebug() << "Primary screen DPI:" << screen->logicalDotsPerInch();
        qDebug() << "Device pixel ratio:" << screen->devicePixelRatio();
        qDebug() << "Physical DPI:" << screen->physicalDotsPerInch();
        qDebug() << "Screen geometry:" << screen->geometry();
        qDebug() << "Available geometry:" << screen->availableGeometry();
    }
    qDebug() << "QT_SCALE_FACTOR env var:" << qgetenv("QT_SCALE_FACTOR");
    qDebug() << "QT_AUTO_SCREEN_SCALE_FACTOR env var:" << qgetenv("QT_AUTO_SCREEN_SCALE_FACTOR");
    qDebug() << "QT_ENABLE_HIGHDPI_SCALING env var:" << qgetenv("QT_ENABLE_HIGHDPI_SCALING");
    qDebug() << "QT_SCALE_FACTOR_ROUNDING_POLICY env var:" << qgetenv("QT_SCALE_FACTOR_ROUNDING_POLICY");
    qDebug() << "QT_FONT_DPI env var:" << qgetenv("QT_FONT_DPI");

    // Debug font scaling information
    QFont defaultFont = QApplication::font();
    qDebug() << "=== Font Scaling Information ===";
    qDebug() << "Default application font:" << defaultFont.family() << "size:" << defaultFont.pointSize() << "pixel size:" << defaultFont.pixelSize();
    QFontMetrics fm(defaultFont);
    qDebug() << "Font metrics height:" << fm.height() << "ascent:" << fm.ascent();

    // Initialize OpenGL 4.6 for maximum performance
    qDebug() << "=== Initializing OpenGL 4.6 for Maximum Performance ===";
    QSurfaceFormat format;

    // Request OpenGL 4.6 Core Profile for latest features and best performance
    format.setVersion(4, 6);
    format.setProfile(QSurfaceFormat::CoreProfile);

    // Enable all performance features
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);

    // High-quality rendering settings
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(8);

    // Enable multisampling based on user settings (loaded early from QSettings)
    format.setSamples(msaaSamples); // Use configured MSAA level

    // Configure VSync based on user preference for balance between responsiveness and smoothness
    // VSync setting loaded from Appearance::enableVSync() (from user settings)
    format.setSwapInterval(enableVSync ? 1 : 0); // 0 = VSync off, 1 = VSync on

    QSurfaceFormat::setDefaultFormat(format);

    qDebug() << "OpenGL 4.6 Core Profile format set:" << format;
    qDebug() << "MSAA samples configured:" << msaaSamples << "(from user settings)";
    qDebug() << "VSync configured:" << (enableVSync ? "ENABLED (smooth playback)" : "DISABLED (responsive editing)");
    qDebug() << "OpenGL module type:" << QOpenGLContext::openGLModuleType();

    a.setApplicationVersion("4.1.0");
    a.setApplicationName("MeowMidiEditor");
    a.setQuitOnLastWindowClosed(true);

    a.setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
    a.setAttribute(Qt::AA_CompressTabletEvents, true);

    // Use more reliable architecture detection
#if defined(__ARCH64__) || defined(_WIN64) || defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64)
    a.setProperty("arch", "64");
#else
    a.setProperty("arch", "32");
#endif

    MidiOutput::init();
    MidiInput::init();

    MainWindow *w;
    if (argc == 2)
        w = new MainWindow(argv[1]);
    else
        w = new MainWindow();
    w->showMaximized();
    return a.exec();
}
