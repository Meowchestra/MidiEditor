#!/bin/sh
#
# -------------- This script is used to create the windows installer. ---------- #
#
# Required environment variables:
#
# MIDIEDITOR_RELEASE_VERSION_ID=2
# MIDIEDITOR_RELEASE_VERSION_STRING=3.1.0
# MIDIEDITOR_PACKAGE_VERSION=1
# MIDIEDITOR_BINARY_WINDOWS=relative/path/to/ProMidiEdit.exe
# INSTALLJAMMER=/path/to/installjammer
# QTDLLS=/folder/of/QtCore.dll
#

# MIDIEDITOR_RELEASE_VERSION_ID=3 MIDIEDITOR_RELEASE_VERSION_STRING=4.0.0 MIDIEDITOR_PACKAGE_VERSION=1 MIDIEDITOR_BINARY_WINDOWS=/d/build-midieditor-Desktop_Qt_5_15_2_MSVC2019_64bit-Release/bin/MidiEditor.exe INSTALLJAMMER="/c/Program Files (x86)/InstallJammer/installjammer" ./create_windows_setup.sh

# Setup folder structure
rm -rf ProMidiEdit-win32

mkdir -p ProMidiEdit-win32
mkdir -p ProMidiEdit-win32/win_root/

# Copy binary
cp -r $(dirname $MIDIEDITOR_BINARY_WINDOWS)/* ProMidiEdit-win32/win_root/

# Copy metronome
cp -R -R ../../packaging/metronome ProMidiEdit-win32/win_root/metronome

# Copy libs
# HINT: If using new lib please add dll to here
cp -t ProMidiEdit-win32/win_root ${QTDLLS}/{Qt5Core,Qt5Gui,Qt5Multimedia,Qt5Network,Qt5Widgets,Qt5Xml}.dll ;

cp -R ../../packaging/windows/windows-installer/. ProMidiEdit-win32;

cd ProMidiEdit-win32

sh "${INSTALLJAMMER}" -DVersion $MIDIEDITOR_RELEASE_VERSION_STRING --build-for-release ./windows-installer.mpi

cd ..

rm -rf releases
mkdir -p releases
cp -a ProMidiEdit-win32/output/ProMidiEdit-$MIDIEDITOR_RELEASE_VERSION_STRING-Setup.exe releases/ProMidiEdit-$MIDIEDITOR_RELEASE_VERSION_STRING-Setup.exe
