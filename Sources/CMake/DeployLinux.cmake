list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Linux")

# Configure deployment variables

set(PROJECT_BINARIES_DEST_DIR "opt/${PROJECT_NAME}")
set(PROJECT_QT_PLUGINS_DEST_DIR "${PROJECT_BINARIES_DEST_DIR}/Plugins")
set(PROJECT_EXECUTABLE "${PROJECT_BINARIES_DEST_DIR}/Drop")
set(PROJECT_SEARCH_PATHS "${QT_LIBRARY_DIR}")

# Install main executable

install(TARGETS Drop DESTINATION "${PROJECT_BINARIES_DEST_DIR}")

# Configure target RPATH fixup

set_target_properties(
    Drop PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    INSTALL_RPATH_USE_LINK_PATH FALSE
)

# Use legacy RPATH instead of RUNPATH. This ensures $ORIGIN expansion works
# for dynamically loaded libraries (dlopen), which is required for Qt plugins.
# RUNPATH does not apply to dlopen() in some glibc versions, leading to
# "library not found" errors.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

# Install Qt configuration files

install(
    FILES "${CMAKE_CURRENT_LIST_DIR}/qt.conf"
    DESTINATION "${PROJECT_BINARIES_DEST_DIR}"
)

# Install required Qt plugins

set(
    REQUIRED_QT_PLUGINS
    "iconengines/libqsvgicon.so"
    "platforms/libqxcb.so"
    "platformthemes/libqgtk3.so"
)

foreach(REQUIRED_QT_PLUGIN ${REQUIRED_QT_PLUGINS})
    get_filename_component(REQUIRED_QT_PLUGIN_DIR ${REQUIRED_QT_PLUGIN} PATH)
    install(
        FILES "${QT_PLUGINS_DIR}/${REQUIRED_QT_PLUGIN}"
        DESTINATION "${PROJECT_QT_PLUGINS_DEST_DIR}/${REQUIRED_QT_PLUGIN_DIR}"
    )
endforeach()

# Configure and install bundle fixup script

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/FixupBundle.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake
    @ONLY
)

install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake)

# Configure and install RPATHs fixup script

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/Linux/FixupRPATHs.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FixupRPATHs.cmake
    @ONLY
)

install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/FixupRPATHs.cmake)

# Install project icon

get_filename_component(PROJECT_ICON_EXT ${PROJECT_ICON} EXT)
set(DESKTOP_ICON_FILENAME "${PROJECT_NAME}${PROJECT_ICON_EXT}")

install(
    FILES "${PROJECT_SOURCE_DIR}/${PROJECT_ICON}"
    DESTINATION "${PROJECT_BINARIES_DEST_DIR}"
    RENAME "${DESKTOP_ICON_FILENAME}"
)

# Configure and install desktop files

set(DESKTOP_NAME "${PROJECT_TITLE}")
set(DESKTOP_ICON "/${PROJECT_BINARIES_DEST_DIR}/${DESKTOP_ICON_FILENAME}")
set(DESKTOP_APPLICATION_EXEC "/${PROJECT_EXECUTABLE}")
set(DESKTOP_COMMENT "${PROJECT_DESCRIPTION}")
set(DESKTOP_CATEGORIES "Network;FileTransfer;")

set(DESKTOP_FILENAME "${PROJECT_NAME}.desktop")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/Linux/Application.desktop.in"
    "${PROJECT_BINARY_DIR}/Application.desktop"
)

install(
    FILES "${PROJECT_BINARY_DIR}/Application.desktop"
    DESTINATION "usr/share/applications"
    RENAME "${DESKTOP_FILENAME}"
)


string(TOLOWER "${PROJECT_NAME}" PACKAGE_NAME)

include(DeployDEB)
include(DeployRPM)
