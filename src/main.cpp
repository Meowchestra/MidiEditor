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
#include <QStyleFactory>
#include <QDir>
#include <QFont>
#include <QSettings>

#include "gui/MainWindow.h"
#include "gui/Appearance.h"
#include "midi/MidiInput.h"
#include "midi/MidiOutput.h"

#include <QFile>
#include <QTextStream>

#include <QMultiMap>
#include <QResource>

#ifdef NO_CONSOLE_MODE
#include <tchar.h>
#include <windows.h>
std::string wstrtostr(const std::wstring& wstr) {
    std::string strTo;
    char* szTo = new char[wstr.length() + 1];
    szTo[wstr.size()] = '\0';
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, szTo, (int)wstr.length(),
                        NULL, NULL);
    strTo = szTo;
    delete[] szTo;
    return strTo;
}
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    int argc = 1;
    char* argv[] = { "", "" };
    std::string str;
    LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (NULL != szArglist && argc > 1) {
        str = wstrtostr(szArglist[1]);
        argv[1] = &str.at(0);
        argc = 2;
    }

#else
int main(int argc, char* argv[]) {
#endif

    // Load high DPI scaling settings before creating QApplication
    // These must be set before QApplication is created
    Appearance::loadEarlySettings();
    bool ignoreSystemScaling = Appearance::ignoreSystemScaling();
    bool useRoundedScaling = Appearance::useRoundedScaling();

    if (ignoreSystemScaling) {
        // Completely disable high DPI scaling (like older Qt5 versions)
        QApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, false);
    } else {
        // Enable high DPI scaling
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

        if (useRoundedScaling) {
            // Use rounded scaling behavior (like Qt5) for sharper rendering
            // Set rounding policy to Round instead of PassThrough (Qt6 default)
            QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);

            // Disable fractional scaling for integer scaling only (100%, 200%, 300%)
            qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
            qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "Round");

            // Use integer-based DPI awareness
            qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
        } else {
            // Use Qt6 default behavior (PassThrough with fractional scaling)
            QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        }
    }

    QApplication a(argc, argv);

    a.setApplicationVersion("4.0.0");
    a.setApplicationName("MeowMidiEditor");
    a.setQuitOnLastWindowClosed(true);

    // Qt6 performance optimizations
    a.setAttribute(Qt::AA_CompressHighFrequencyEvents, true);  // Compress mouse/paint events
    a.setAttribute(Qt::AA_CompressTabletEvents, true);        // Compress tablet events

    // Qt6 RHI backend selection based on user settings
    QSettings settings(QString("MidiEditor"), QString("NONE"));
    QString preferredBackend = settings.value("rendering/backend", "auto").toString();
    bool hardwareAcceleration = settings.value("rendering/hardware_acceleration", true).toBool();

    if (!QApplication::arguments().contains("--no-acceleration") && hardwareAcceleration) {
        QString backend;

        if (preferredBackend == "auto") {
            // Auto-select best backend for platform
            #ifdef Q_OS_WIN
                backend = "d3d11";  // Best performance on Windows
            #elif defined(Q_OS_MACOS)
                backend = "metal";  // Best performance on macOS
            #else
                backend = "opengl"; // Most compatible on Linux
            #endif
        } else {
            backend = preferredBackend;
        }

        qputenv("QSG_RHI_BACKEND", backend.toUtf8());
        qputenv("QSG_RHI_DEBUG_LAYER", "0");  // Disable debug layer for performance
        a.setAttribute(Qt::AA_UseOpenGLES, false);  // Use full OpenGL when needed
    } else {
        // Force software rendering if acceleration disabled
        qputenv("QSG_RHI_BACKEND", "software");
    }

// Use more reliable architecture detection
#if defined(__ARCH64__) || defined(_WIN64) || defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64)
    a.setProperty("arch", "64");
#else
    a.setProperty("arch", "32");
#endif

    MidiOutput::init();
    MidiInput::init();

    MainWindow* w;
    if (argc == 2)
        w = new MainWindow(argv[1]);
    else
        w = new MainWindow();
    w->showMaximized();
    return a.exec();
}
