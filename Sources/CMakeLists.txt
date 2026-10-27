cmake_minimum_required(VERSION 3.20)

project(Drop VERSION 0.1 LANGUAGES CXX)

set(PROJECT_TITLE "Drop")
set(PROJECT_DESCRIPTION "Drop files from phones and other devices to this computer over the local network.")

set(PROJECT_DEVELOPER_TITLE "Andrew N. Markin")
set(PROJECT_DEVELOPER_EMAIL "andrew.n.markin@gmail.com")
set(PROJECT_DEVELOPER "${PROJECT_DEVELOPER_TITLE} <${PROJECT_DEVELOPER_EMAIL}>")

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
    PROJECT_GITHUB_REPOSITORY="${PROJECT_GITHUB_REPOSITORY}"
)

# Find required Qt libraries

find_package(Qt6 REQUIRED COMPONENTS Widgets Network HttpServer)

# Bundled third party libraries

add_subdirectory(ThirdParty/QrCodeGen)

# Add the target

add_executable(
    Drop WIN32
    Drop.cpp
    Drop.h
    LocalNetwork.cpp
    LocalNetwork.h
    Resources.qrc
    Storage.cpp
    Storage.h
)

target_include_directories(Drop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
    Drop PRIVATE
    DropDefinitions
    Qt6::Widgets
    Qt6::Network
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
