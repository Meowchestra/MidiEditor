cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

set(MIDIEDITOR_RELEASE_VERSION_STRING "4.0.0")
project(MidiEditor VERSION ${MIDIEDITOR_RELEASE_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    set(PLAT_WINDOWS TRUE)
elseif(UNIX AND NOT APPLE)
    set(PLAT_LINUX TRUE)
elseif(APPLE)
    set(PLAT_MACOSX TRUE)
endif()

# Options
option(GENERATE_REPOSITORY "generate repository to update online repositories" OFF)
if(PLAT_LINUX)
    execute_process(COMMAND lsb_release -si OUTPUT_VARIABLE LINUX_DISTRO OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(LINUX_DISTRO STREQUAL "Ubuntu")
        option(LIBRARIES_FROM_APT "use libraries from apt rather than package manager" OFF)
    endif()
endif()

# Qt6 detection with fallbacks
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{QT_ROOT_DIR} $ENV{Qt6_DIR})

# Add common Qt installation paths
if(WIN32)
    # Common Qt installation paths on Windows
    # Prioritize MinGW builds if using GCC compiler
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        file(GLOB QT_MINGW "C:/Qt/6.*/mingw*")
        if(QT_MINGW)
            list(GET QT_MINGW -1 QT_MINGW_LATEST)
            set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${QT_MINGW_LATEST}")
            message(STATUS "Found Qt MinGW installation: ${QT_MINGW_LATEST}")
        endif()
    endif()

    # Fallback to MSVC builds
    file(GLOB QT_VERSIONS "C:/Qt/6.*")
    if(QT_VERSIONS)
        list(GET QT_VERSIONS -1 QT_LATEST_VERSION)  # Get the latest version
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${QT_LATEST_VERSION}/msvc2022_64")
        message(STATUS "Found Qt installation: ${QT_LATEST_VERSION}/msvc2022_64")
    endif()

    # Also try other common compiler versions
    file(GLOB QT_MSVC2019 "C:/Qt/6.*/msvc2019_64")
    if(QT_MSVC2019)
        list(GET QT_MSVC2019 -1 QT_MSVC2019_LATEST)
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${QT_MSVC2019_LATEST}")
        message(STATUS "Found Qt MSVC2019 installation: ${QT_MSVC2019_LATEST}")
    endif()
endif()

# Try to find Qt6 with different approaches
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network Xml Multimedia MultimediaWidgets)

if(NOT Qt6_FOUND)
    # Try with Qt6_DIR environment variable
    if(DEFINED ENV{Qt6_DIR})
        set(Qt6_DIR $ENV{Qt6_DIR})
        find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Network Xml Multimedia MultimediaWidgets)
    endif()
endif()

if(NOT Qt6_FOUND)
    # Try with QT_ROOT_DIR environment variable
    if(DEFINED ENV{QT_ROOT_DIR})
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{QT_ROOT_DIR})
        find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Network Xml Multimedia MultimediaWidgets)
    endif()
endif()

if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please set Qt6_DIR or QT_ROOT_DIR environment variable to your Qt installation directory, or install Qt6 to C:/Qt/")
endif()

# Debug Qt6 information
message(STATUS "Qt6 found: ${Qt6_FOUND}")
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 directory: ${Qt6_DIR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
get_target_property(QT6_CORE_LOCATION Qt6::Core LOCATION)
message(STATUS "Qt6::Core location: ${QT6_CORE_LOCATION}")

# Check for compiler/Qt compatibility
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(WARNING "Using MinGW compiler with Qt6. Make sure Qt6 was compiled with MinGW, not MSVC.")
    message(STATUS "If you encounter linking errors, try using Qt6 compiled for MinGW or switch to MSVC compiler.")
endif()

# Find Vulkan SDK
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Added Vulkan SDK include directory: ${Vulkan_INCLUDE_DIRS}")
else()
    message(STATUS "Vulkan SDK not found - Vulkan backend may not be available")
endif()

# Enable Qt6 MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Ensure proper Qt6 setup
if(Qt6_FOUND)
    qt6_standard_project_setup()
endif()

# Main target
add_executable(MidiEditor)

target_link_libraries(MidiEditor PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Xml
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)

# Additional linking for MinGW compatibility
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(MidiEditor PRIVATE -Wl,--allow-multiple-definition)
endif()

# Qt RHI for hardware acceleration
find_package(Qt6 QUIET COMPONENTS GuiPrivate)
if(Qt6GuiPrivate_FOUND)
    target_link_libraries(MidiEditor PRIVATE Qt6::GuiPrivate)
endif()

# Include directories
target_include_directories(MidiEditor PRIVATE
    src
    src/gui
    src/midi
    src/MidiEvent
    src/protocol
    src/tool
    src/third-party
)
message(STATUS "Added project include directories")

# Hardware acceleration shaders
set(COMPILED_SHADERS_DIR "src/shaders/compiled")
if(EXISTS "${CMAKE_SOURCE_DIR}/${COMPILED_SHADERS_DIR}")
    message(STATUS "Found compiled shaders at ${COMPILED_SHADERS_DIR}")
else()
    message(WARNING "No compiled shaders found at ${COMPILED_SHADERS_DIR}")
    message(STATUS "Hardware acceleration will use fallback rendering.")
    message(STATUS "Run GitHub Actions 'Compile Shaders' workflow to generate shaders")
endif()

# Vulkan includes
if(Vulkan_FOUND)
    target_include_directories(MidiEditor PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Qt RHI source paths
if(DEFINED ENV{QT_RHI_SOURCE_PATH})
    file(TO_CMAKE_PATH "$ENV{QT_RHI_SOURCE_PATH}" QT_RHI_SOURCE_NORMALIZED)
    target_include_directories(MidiEditor PRIVATE ${QT_RHI_SOURCE_NORMALIZED})
    message(STATUS "Added Qt RHI source paths: ${QT_RHI_SOURCE_NORMALIZED}")
    message(STATUS "Expected RHI header location: ${QT_RHI_SOURCE_NORMALIZED}/rhi/qrhi_p.h")

    if(DEFINED ENV{QT_ROOT_DIR})
        # Try to find versioned private headers dynamically
        file(GLOB QT_VERSIONED_PRIVATE "$ENV{QT_ROOT_DIR}/include/QtGui/*/QtGui/private")
        if(QT_VERSIONED_PRIVATE)
            list(GET QT_VERSIONED_PRIVATE 0 QT_VERSIONED_DIR)
            target_include_directories(MidiEditor PRIVATE "${QT_VERSIONED_DIR}")
            message(STATUS "Added versioned Qt RHI private headers: ${QT_VERSIONED_DIR}")
        endif()
    endif()
elseif(DEFINED ENV{QT_ROOT_DIR} OR DEFINED ENV{Qt6_DIR})
    if(DEFINED ENV{QT_ROOT_DIR})
        set(QT_DIR "$ENV{QT_ROOT_DIR}")
    else()
        set(QT_DIR "$ENV{Qt6_DIR}")
    endif()

    # Add standard private headers
    if(EXISTS "${QT_DIR}/include/QtGui/private")
        target_include_directories(MidiEditor PRIVATE "${QT_DIR}/include/QtGui/private")
        message(STATUS "Added Qt RHI private headers from: ${QT_DIR}/include/QtGui/private")
    endif()

    # Try to find versioned private headers dynamically
    file(GLOB QT_VERSIONED_PRIVATE "${QT_DIR}/include/QtGui/*/QtGui/private")
    if(QT_VERSIONED_PRIVATE)
        list(GET QT_VERSIONED_PRIVATE 0 QT_VERSIONED_DIR)
        target_include_directories(MidiEditor PRIVATE "${QT_VERSIONED_DIR}")
        message(STATUS "Added versioned Qt RHI private headers: ${QT_VERSIONED_DIR}")
    endif()
else()
    if(DEFINED Qt6_DIR)
        get_filename_component(QT_DETECTED_DIR "${Qt6_DIR}/../../.." ABSOLUTE)

        # Add standard private headers
        if(EXISTS "${QT_DETECTED_DIR}/include/QtGui/private")
            target_include_directories(MidiEditor PRIVATE "${QT_DETECTED_DIR}/include/QtGui/private")
            message(STATUS "Added Qt RHI private headers from detected Qt: ${QT_DETECTED_DIR}/include/QtGui/private")
        endif()

        file(GLOB QT_VERSIONED_PRIVATE "${QT_DETECTED_DIR}/include/QtGui/*/QtGui/private")
        if(QT_VERSIONED_PRIVATE)
            list(GET QT_VERSIONED_PRIVATE 0 QT_VERSIONED_DIR)
            target_include_directories(MidiEditor PRIVATE "${QT_VERSIONED_DIR}")
            message(STATUS "Added versioned Qt RHI private headers from detected Qt: ${QT_VERSIONED_DIR}")
        endif()
    else()
        message(WARNING "No Qt directory found, Qt RHI headers may not be available")
    endif()
endif()

# Source files - be more specific to avoid test/doc/example files
file(GLOB SOURCES_MAIN "src/*.cpp")
file(GLOB SOURCES_MIDIEVENT "src/MidiEvent/*.cpp")
file(GLOB SOURCES_GUI "src/gui/*.cpp")
file(GLOB SOURCES_PROTOCOL "src/protocol/*.cpp")
file(GLOB SOURCES_TOOL "src/tool/*.cpp")

# For midi directory, be selective to avoid test files
file(GLOB SOURCES_MIDI "src/midi/*.cpp")

# Combine all sources
set(SOURCES
    ${SOURCES_MAIN}
    ${SOURCES_MIDIEVENT}
    ${SOURCES_GUI}
    ${SOURCES_MIDI}
    ${SOURCES_PROTOCOL}
    ${SOURCES_TOOL}
)

# Add only main rtmidi files if they exist (not tests/docs/examples)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/midi/rtmidi/RtMidi.cpp")
    list(APPEND SOURCES "src/midi/rtmidi/RtMidi.cpp")
    # Add rtmidi include directory so RtMidi.h can be found
    target_include_directories(MidiEditor PRIVATE "src/midi/rtmidi")
    message(STATUS "Added RtMidi.cpp and include directory")
else()
    message(WARNING "RtMidi.cpp not found at src/midi/rtmidi/RtMidi.cpp")
endif()

# Remove any test, example, or doc files that might have been included
list(FILTER SOURCES EXCLUDE REGEX ".*/tests/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/test/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/examples/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/example/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/doc/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/docs/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/android/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/samples/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/old/.*")

# Headers - be more specific to avoid test/doc/example files
file(GLOB HEADERS_MAIN "src/*.h")
file(GLOB HEADERS_MIDIEVENT "src/MidiEvent/*.h")
file(GLOB HEADERS_GUI "src/gui/*.h")
file(GLOB HEADERS_PROTOCOL "src/protocol/*.h")
file(GLOB HEADERS_TOOL "src/tool/*.h")

# For midi directory, be selective to avoid test files
file(GLOB HEADERS_MIDI "src/midi/*.h")

# Combine all headers
set(HEADERS
    ${HEADERS_MAIN}
    ${HEADERS_MIDIEVENT}
    ${HEADERS_GUI}
    ${HEADERS_MIDI}
    ${HEADERS_PROTOCOL}
    ${HEADERS_TOOL}
)

if(EXISTS "${CMAKE_SOURCE_DIR}/src/midi/rtmidi/RtMidi.h")
    list(APPEND HEADERS "src/midi/rtmidi/RtMidi.h")
    message(STATUS "Added RtMidi.h")
else()
    message(WARNING "RtMidi.h not found at src/midi/rtmidi/RtMidi.h")
endif()

# Remove any test, example, or doc files that might have been included
list(FILTER HEADERS EXCLUDE REGEX ".*/tests/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/test/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/examples/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/example/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/doc/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/docs/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/android/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/samples/.*")
list(FILTER HEADERS EXCLUDE REGEX ".*/old/.*")

# Check if we have any source files
list(LENGTH SOURCES SOURCE_COUNT)
if(SOURCE_COUNT EQUAL 0)
    message(FATAL_ERROR "No source files found! Please check that the src directory exists and contains .cpp files.")
endif()

message(STATUS "Found ${SOURCE_COUNT} source files")

target_sources(MidiEditor PRIVATE ${SOURCES} ${HEADERS})

# Qt resources
if(EXISTS "${CMAKE_SOURCE_DIR}/resources.qrc")
    target_sources(MidiEditor PRIVATE resources.qrc)
    message(STATUS "Added Qt resources from resources.qrc")
else()
    message(WARNING "resources.qrc not found - some resources may be missing")
endif()

# Architecture defines
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(MidiEditor PRIVATE __ARCH64__)
endif()

# Version defines
string(TIMESTAMP CURRENT_DATE "%x")
target_compile_definitions(MidiEditor PRIVATE
        MIDIEDITOR_RELEASE_VERSION_ID_DEF=0
        MIDIEDITOR_RELEASE_DATE_DEF="${CURRENT_DATE}"
        MIDIEDITOR_RELEASE_VERSION_STRING_DEF="${MIDIEDITOR_RELEASE_VERSION_STRING}"
)

# Platform-specific settings
if(PLAT_LINUX)
    target_compile_definitions(MidiEditor PRIVATE
            __LINUX_ALSASEQ__
            __LINUX_ALSA__
    )
    target_link_libraries(MidiEditor PRIVATE asound)
elseif(PLAT_WINDOWS)
    target_compile_definitions(MidiEditor PRIVATE __WINDOWS_MM__)
    target_link_libraries(MidiEditor PRIVATE winmm)

    # Add Windows resource file if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/midieditor.rc")
        target_sources(MidiEditor PRIVATE midieditor.rc)
        message(STATUS "Added Windows resource file: midieditor.rc")
    else()
        message(WARNING "midieditor.rc not found - Windows resources may be missing")
    endif()

    set_target_properties(MidiEditor PROPERTIES
            WIN32_EXECUTABLE TRUE
    )

    # Set appropriate linker flags based on compiler
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MinGW/GCC syntax
        target_link_options(MidiEditor PRIVATE -mwindows)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # MSVC syntax
        target_link_options(MidiEditor PRIVATE /SUBSYSTEM:WINDOWS)
    endif()
elseif(PLAT_MACOSX)
    target_compile_definitions(MidiEditor PRIVATE __MACOSX_CORE__)
    target_link_libraries(MidiEditor PRIVATE
            "-framework CoreMidi"
            "-framework CoreAudio"
            "-framework CoreFoundation"
    )
endif()

# Output directory
set_target_properties(MidiEditor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Qt deployment for development builds (Windows only)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXECUTABLE)
        # Deploy Qt libraries to build directory for debugging
        add_custom_command(TARGET MidiEditor POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --debug
                --compiler-runtime
                --force
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:MidiEditor>
            COMMENT "Deploying Qt libraries for debug"
        )

        # Ensure platforms plugin is deployed
        add_custom_command(TARGET MidiEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:MidiEditor>/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${Qt6_DIR}/../../../plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll
                $<TARGET_FILE_DIR:MidiEditor>/platforms/
            COMMENT "Ensuring Qt platform plugins are available"
        )
    else()
        message(WARNING "windeployqt not found - attempting manual Qt deployment")

        # Manual deployment of essential Qt DLLs
        set(QT_BIN_DIR ${Qt6_DIR}/../../../bin)
        set(QT_PLUGINS_DIR ${Qt6_DIR}/../../../plugins)

        # Copy essential Qt DLLs
        add_custom_command(TARGET MidiEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT_BIN_DIR}/Qt6Core$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6Gui$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6Widgets$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6Network$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6Xml$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6Multimedia$<$<CONFIG:Debug>:d>.dll
                ${QT_BIN_DIR}/Qt6MultimediaWidgets$<$<CONFIG:Debug>:d>.dll
                $<TARGET_FILE_DIR:MidiEditor>/
            COMMENT "Manually copying Qt DLLs"
        )

        # Copy platform plugins
        add_custom_command(TARGET MidiEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:MidiEditor>/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT_PLUGINS_DIR}/platforms/qwindows$<$<CONFIG:Debug>:d>.dll
                $<TARGET_FILE_DIR:MidiEditor>/platforms/
            COMMENT "Manually copying Qt platform plugins"
        )
    endif()
endif()

# Installation
set(INSTALL_DIR "packaging/org.midieditor.midieditor/data/")
set(BIN_DIR "${INSTALL_DIR}/bin")
set(PLUGIN_DIR "${BIN_DIR}/plugins")

install(TARGETS MidiEditor DESTINATION "${BIN_DIR}")

# Install metronome file if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/run_environment/metronome/metronome-01.wav")
    install(FILES "run_environment/metronome/metronome-01.wav" DESTINATION "${INSTALL_DIR}/metronome")
else()
    message(WARNING "metronome-01.wav not found - metronome functionality may be limited")
endif()

if(PLAT_WINDOWS)
    # Qt deployment
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --plugindir ${PLUGIN_DIR}
                --libdir ${BIN_DIR}
                \${CMAKE_INSTALL_PREFIX}/${BIN_DIR}/MidiEditor.exe
            )
            file(GLOB DSENGINE_FILES \"\${CMAKE_INSTALL_PREFIX}/${BIN_DIR}/**/dsengine.dll\")
            foreach(FILE \${DSENGINE_FILES})
                file(REMOVE \${FILE})
            endforeach()
        ")
    endif()
endif()

# Manual target (Windows only)
if(PLAT_WINDOWS)
    add_custom_target(manual ALL)
    install(DIRECTORY "manual/" DESTINATION "packaging/org.midieditor.manual/data/manual")
endif()

# Installer target
if(PLAT_WINDOWS OR PLAT_LINUX)
    add_custom_target(installer DEPENDS MidiEditor)

    if(PLAT_WINDOWS)
        add_dependencies(installer manual)

        find_program(BINARYCREATOR_EXECUTABLE binarycreator)
        find_program(REPOGEN_EXECUTABLE repogen)

        if(BINARYCREATOR_EXECUTABLE AND REPOGEN_EXECUTABLE)
            if(GENERATE_REPOSITORY)
                add_custom_command(TARGET installer POST_BUILD
                        COMMAND ${REPOGEN_EXECUTABLE} --update-new-components --packages packaging ${CMAKE_BINARY_DIR}/website/repository
                        COMMAND ${BINARYCREATOR_EXECUTABLE} --config scripts/packaging/windows/config.xml --packages packaging ${CMAKE_BINARY_DIR}/website/MidiEditor.exe
                        COMMAND ${CMAKE_COMMAND} -E copy_directory manual ${CMAKE_BINARY_DIR}/website
                        COMMENT "Generating repository and online installer"
                )
            else()
                add_custom_command(TARGET installer POST_BUILD
                        COMMAND ${BINARYCREATOR_EXECUTABLE} --config scripts/packaging/windows/config.xml --packages packaging packaging/Install.exe
                        COMMENT "Generating offline installer"
                )
            endif()
        endif()

    elseif(PLAT_LINUX AND LINUX_DISTRO STREQUAL "Ubuntu")
        install(FILES "scripts/packaging/debian/MidiEditor.desktop" DESTINATION "usr/share/applications")
        install(FILES "scripts/packaging/debian/logo48.png" DESTINATION "usr/share/pixmaps")
        install(FILES "scripts/packaging/debian/copyright" DESTINATION "usr/share/doc/midieditor")

        configure_file("scripts/packaging/debian/control" "${CMAKE_BINARY_DIR}/control" @ONLY)
        install(FILES "${CMAKE_BINARY_DIR}/control" DESTINATION "DEBIAN")

        add_custom_command(TARGET installer POST_BUILD
                COMMAND chmod 755 ${CMAKE_INSTALL_PREFIX}
                COMMAND find ${CMAKE_INSTALL_PREFIX} -type d -exec chmod 755 {} +
                COMMAND find ${CMAKE_INSTALL_PREFIX} -type f -exec chmod 644 {} +
                COMMAND chmod +x ${CMAKE_INSTALL_PREFIX}/${BIN_DIR}/MidiEditor
                COMMAND fakeroot dpkg-deb --build ${CMAKE_INSTALL_PREFIX}
                COMMENT "Building Debian package"
        )
    endif()
endif()