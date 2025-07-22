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

#include "gui/MainWindow.h"
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

    QApplication a(argc, argv);

    a.setApplicationVersion("4.0.0");
    a.setApplicationName("Meow MidiEditor");
    a.setQuitOnLastWindowClosed(true);

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
