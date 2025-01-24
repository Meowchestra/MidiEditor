cmake_minimum_required(VERSION 3.16)
project(ProMidEdit VERSION 3.9.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add this section to help find Qt6
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.1/msvc2022_64/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS 
    Core
    Gui
    Widgets
    Network
    Xml
    Multimedia
    MultimediaWidgets
)

# Collect source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)

# Add Qt resource files
set(RESOURCE_FILES resources.qrc)
qt_add_resources(RESOURCES ${RESOURCE_FILES})

# Add Windows RC file if on Windows
if(WIN32)
    set(RC_FILE midieditor.rc)
endif()

# Create executable
add_executable(${PROJECT_NAME} 
    ${SOURCES}
    ${RESOURCES}
    ${RC_FILE}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MIDIEDITOR_RELEASE_VERSION_ID_DEF=0
    MIDIEDITOR_RELEASE_DATE_DEF="${CMAKE_SYSTEM_TIME}"
    MIDIEDITOR_RELEASE_VERSION_STRING_DEF="${PROJECT_VERSION}"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE __ARCH64__)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Installation rules
set(INSTALL_DIR "packaging/org.midieditor.midieditor/data")
set(BIN_DIR "${INSTALL_DIR}/bin")
set(PLUGIN_DIR "${BIN_DIR}/plugins")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${BIN_DIR}
)