cmake_minimum_required(VERSION 3.20)

project(Drop VERSION 0.1 LANGUAGES CXX)

set(PROJECT_TITLE "Drop")
set(PROJECT_DESCRIPTION "Drop files from phones and other devices to this computer over the local network.")

set(PROJECT_DEVELOPER_TITLE "Andrew N. Markin")
set(PROJECT_DEVELOPER_EMAIL "andrew.n.markin@gmail.com")
set(PROJECT_DEVELOPER "${PROJECT_DEVELOPER_TITLE} <${PROJECT_DEVELOPER_EMAIL}>")

# Names the folder of the settings
set(PROJECT_ORGANIZATION "Mayfleet")

set(ORIGINAL_YEAR "2026")
string(TIMESTAMP CURRENT_YEAR "%Y")

if(ORIGINAL_YEAR EQUAL CURRENT_YEAR)
    set(COPYRIGHT_YEARS "${ORIGINAL_YEAR}")
else()
    set(COPYRIGHT_YEARS "${ORIGINAL_YEAR}-${CURRENT_YEAR}")
endif()

set(PROJECT_COPYRIGHT "Copyright (C) ${COPYRIGHT_YEARS} ${PROJECT_DEVELOPER_TITLE}. All rights reserved.")

set(PROJECT_GITHUB_REPOSITORY "https://github.com/andrew-markin/drop")
set(PROJECT_HOME_URL "${PROJECT_GITHUB_REPOSITORY}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif()

# Project information macros, shared by the application and unit tests

add_library(DropDefinitions INTERFACE)

target_compile_definitions(
    DropDefinitions INTERFACE
    PROJECT_NAME="${PROJECT_NAME}"
    PROJECT_TITLE="${PROJECT_TITLE}"
    PROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}"
    PROJECT_COPYRIGHT="${PROJECT_COPYRIGHT}"
    PROJECT_VERSION="${PROJECT_VERSION}"
    PROJECT_DEVELOPER_TITLE="${PROJECT_DEVELOPER_TITLE}"
    PROJECT_ORGANIZATION="${PROJECT_ORGANIZATION}"
    PROJECT_GITHUB_REPOSITORY="${PROJECT_GITHUB_REPOSITORY}"
)

# Find required Qt libraries

find_package(Qt6 REQUIRED COMPONENTS Widgets Network HttpServer)

# Notifications that can be clicked, see DesktopNotifier.h. Qt uses it for the tray icon there anyway.
# The icon theme gets PNG files of the icon, rendered from the SVG, see Tools/IconRenderer.cpp.
if(LINUX)
    find_package(Qt6 REQUIRED COMPONENTS DBus Svg)
endif()

# Third party libraries

find_package(Sodium REQUIRED)

get_filename_component(QT_BINARY_DIR "${QT6_INSTALL_PREFIX}/bin" ABSOLUTE)
get_filename_component(QT_LIBRARY_DIR "${QT6_INSTALL_PREFIX}/lib" ABSOLUTE)
get_filename_component(QT_PLUGINS_DIR "${QT6_INSTALL_PREFIX}/plugins" ABSOLUTE)

# Bundled third party libraries

add_subdirectory(ThirdParty/QrCodeGen)

# Add the target

add_executable(
    Drop WIN32
    AlertWidget.cpp
    AlertWidget.h
    Colors.cpp
    Colors.h
    Crypto.cpp
    Crypto.h
    DesktopNotifier.cpp
    DesktopNotifier.h
    Drop.cpp
    Drop.h
    LocalNetwork.cpp
    LocalNetwork.h
    NetworkComboBox.cpp
    NetworkComboBox.h
    QrCodeDialog.cpp
    QrCodeDialog.h
    QrCodeWidget.cpp
    QrCodeWidget.h
    TrayIcon.cpp
    TrayIcon.h
    SingleInstance.cpp
    SingleInstance.h
    Resources.qrc
    Storage.cpp
    Storage.h
    Translator.cpp
    Translator.h
    Uploads.cpp
    Uploads.h
    UploadsDelegate.cpp
    UploadsDelegate.h
    UploadsEstimate.cpp
    UploadsEstimate.h
    UploadsHeaderView.cpp
    UploadsHeaderView.h
    UploadsModel.cpp
    UploadsModel.h
    UploadsView.cpp
    UploadsView.h
    WebServer.cpp
    WebServer.h
)

target_include_directories(Drop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(EmbedWeb)
embed_web(Drop)

if(WIN32)
    set(PROJECT_ICON "Drop.ico")

    # Configure executable resources

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Drop.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/Drop.rc
        @ONLY
    )

    target_sources(
        Drop PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/Drop.rc
    )
elseif(LINUX)
    set(PROJECT_ICON "Drop.svg")

    # The same sizes as IconSizes in Drop.cpp
    set(PROJECT_ICON_SIZES 16 22 24 32 48 64 128 256)
    set(PROJECT_ICONS_DIR "${CMAKE_CURRENT_BINARY_DIR}/Icons")

    add_executable(IconRenderer Tools/IconRenderer.cpp)
    target_link_libraries(IconRenderer PRIVATE Qt6::Gui Qt6::Svg)

    set(PROJECT_ICON_FILES)

    foreach(PROJECT_ICON_SIZE ${PROJECT_ICON_SIZES})
        list(APPEND PROJECT_ICON_FILES "${PROJECT_ICONS_DIR}/${PROJECT_ICON_SIZE}/${PROJECT_NAME}.png")
    endforeach()

    add_custom_command(
        OUTPUT ${PROJECT_ICON_FILES}
        COMMAND IconRenderer "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_ICON}" "${PROJECT_ICONS_DIR}"
                "${PROJECT_NAME}.png" ${PROJECT_ICON_SIZES}
        DEPENDS IconRenderer "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_ICON}"
        COMMENT "Rendering application icons"
        VERBATIM
    )

    add_custom_target(Icons ALL DEPENDS ${PROJECT_ICON_FILES})
endif()

target_link_libraries(
    Drop PRIVATE
    DropDefinitions
    Qt6::Widgets
    Qt6::Network
    Qt6::HttpServer
    QrCodeGen
    Sodium::Sodium
)

if (WIN32)
    target_compile_options(
        Drop PRIVATE
        /utf-8
        /nologo
    )
    target_compile_definitions(
        Drop PRIVATE
        UNICODE
        _UNICODE
    )
    set_target_properties(
        Drop PROPERTIES
        MSVC_WCHAR_T_AS_BUILTIN_TYPE ON
    )
elseif(LINUX)
    target_link_libraries(
        Drop PRIVATE
        Qt6::DBus
    )
    target_compile_options(
        Drop PRIVATE
        -march=x86-64
        -mtune=generic
        -Wno-padded
    )
    target_link_options(
        Drop PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

# Unit tests are optional and not a part of release packages

option(DROP_BUILD_TESTS "Build unit tests" OFF)

if(DROP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

include(Deploy)
