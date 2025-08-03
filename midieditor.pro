TEMPLATE = app
TARGET = MidiEditor

# Force 64-bit build by default
CONFIG += force_64bit
force_64bit {
    DEFINES += __ARCH64__
    message(Forcing 64-bit build)
}

QT += widgets \
    core \
    gui \
    network \
    xml \
    multimedia

# Qt RHI for hardware acceleration
QT += gui-private

HEADERS += $$files(**.h, true)
HEADERS -= $$files(src/midi/rtmidi/**.h, true)
HEADERS += src/midi/rtmidi/RtMidi.h
SOURCES += $$files(**.cpp, true)
SOURCES -= $$files(src/midi/rtmidi/**.cpp, true)
SOURCES += src/midi/rtmidi/RtMidi.cpp
CONFIG += static
FORMS += 
RESOURCES += resources.qrc
# Enhanced architecture detection
ARCH_FORCE = $$(OVERRIDE_ARCH)
contains(ARCH_FORCE, 64){
    DEFINES += __ARCH64__
    message(arch forced to 64 bit)
} else {
    contains(ARCH_FORCE, 32){
        message(arch forced to 32 bit)
    } else {
        # Check multiple ways to detect 64-bit architecture
        contains(QMAKE_HOST.arch, x86_64) | contains(QMAKE_HOST.arch, amd64) | contains(QT_ARCH, x86_64) | contains(QT_ARCH, amd64) {
            DEFINES += __ARCH64__
            message(arch recognized as 64 bit)
        } else {
            # Check if we're using a 64-bit Qt installation
            contains(QMAKE_TARGET.arch, x86_64) | contains(QMAKE_TARGET.arch, amd64) {
                DEFINES += __ARCH64__
                message(arch recognized as 64 bit via target arch)
            } else {
                message(arch recognized as 32 bit)
            }
        }
    }
}

## Obtain release version info. We get this from the environment variables.
## To set them, use
##
## $ export MIDIEDITOR_RELEASE_VERSION_STRING=3.1.4
## $ export MIDIEDITOR_RELEASE_VERSION_ID=4
## $ export MIDIEDITOR_RELEASE_DATE=1970-01-01
##
message(MIDIEDITOR_RELEASE_VERSION_STRING=$$(MIDIEDITOR_RELEASE_VERSION_STRING))
MIDIEDITOR_RELEASE_VERSION_STRING_QMAKE=$$(MIDIEDITOR_RELEASE_VERSION_STRING)
isEmpty(MIDIEDITOR_RELEASE_VERSION_STRING_QMAKE) {
    MIDIEDITOR_RELEASE_VERSION_STRING_QMAKE=Unofficial
}
message(Release version is set to $$MIDIEDITOR_RELEASE_VERSION_STRING_QMAKE)
DEFINES += MIDIEDITOR_RELEASE_VERSION_STRING_DEF=$$MIDIEDITOR_RELEASE_VERSION_STRING_QMAKE

message(MIDIEDITOR_RELEASE_VERSION_ID=$$(MIDIEDITOR_RELEASE_VERSION_ID))
MIDIEDITOR_RELEASE_VERSION_ID_QMAKE=$$(MIDIEDITOR_RELEASE_VERSION_ID)
isEmpty(MIDIEDITOR_RELEASE_VERSION_ID_QMAKE) {
    MIDIEDITOR_RELEASE_VERSION_ID_QMAKE=4
}
message(Release version id is set to $$MIDIEDITOR_RELEASE_VERSION_ID_QMAKE)
DEFINES += MIDIEDITOR_RELEASE_VERSION_ID_DEF=$$MIDIEDITOR_RELEASE_VERSION_ID_QMAKE

MIDIEDITOR_RELEASE_DATE_QMAKE=$$(MIDIEDITOR_RELEASE_DATE)
isEmpty(MIDIEDITOR_RELEASE_DATE_QMAKE) {
    win32 {
        MIDIEDITOR_RELEASE_DATE_QMAKE=\"$$quote($$system("echo %date:~4,10%"))\"
    }
    else {
        MIDIEDITOR_RELEASE_DATE_QMAKE=\"$$quote($$system(date))\"
    }
}
DEFINES += MIDIEDITOR_RELEASE_DATE_DEF=$$MIDIEDITOR_RELEASE_DATE_QMAKE
message(Release date is set to $$MIDIEDITOR_RELEASE_DATE_QMAKE)

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
    # Set Windows subsystem to GUI to prevent console window
    QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS
}

macx: {
    DEFINES += __MACOSX_CORE__
    LIBS += -framework CoreMidi -framework CoreAudio -framework CoreFoundation
    CONFIG += release
    OBJECTS_DIR = .tmp
    MOC_DIR = .tmp
    ICON = midieditor.icns
}
