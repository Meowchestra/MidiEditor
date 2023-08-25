TEMPLATE = app
TARGET = ProMidEdit
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += core \
    gui \
    network \
    xml \
    multimedia
HEADERS += $$files(**.h, true)
HEADERS -= $$files(src/midi/rtmidi/**.h, true)
HEADERS += src/midi/rtmidi/RtMidi.h
SOURCES += $$files(**.cpp, true)
SOURCES -= $$files(src/midi/rtmidi/**.cpp, true)
SOURCES += src/midi/rtmidi/RtMidi.cpp
CONFIG += static
FORMS += 
RESOURCES += resources.qrc
ARCH_FORCE = $$(OVERRIDE_ARCH)
contains(ARCH_FORCE, 64){
    DEFINES += __ARCH64__
    message(arch forced to 64 bit)
} else {
    contains(ARCH_FORCE, 32){
	message(arch forced to 32 bit)
    } else {
	contains(QMAKE_HOST.arch, x86_64):{
	    DEFINES += __ARCH64__
	    message(arch recognized as 64 bit)
	} else {
	    message(arch recognized as 32 bit)
        }
    }
}

unix:!macx {
    DEFINES += __LINUX_ALSASEQ__
    DEFINES += __LINUX_ALSA__
    LIBS += -lasound
    #CONFIG += release
    OBJECTS_DIR = .tmp
    MOC_DIR = .tmp
}

win32: {
    DEFINES += __WINDOWS_MM__
    LIBS += -lwinmm
    CONFIG += static release
    RC_FILE = midieditor.rc
    OBJECTS_DIR = .tmp
    MOC_DIR = .tmp
    Release:DESTDIR = bin
}

macx: {
    DEFINES += __MACOSX_CORE__
    LIBS += -framework CoreMidi -framework CoreAudio -framework CoreFoundation
    CONFIG += release
    OBJECTS_DIR = .tmp
    MOC_DIR = .tmp
    ICON = midieditor.icns
}
