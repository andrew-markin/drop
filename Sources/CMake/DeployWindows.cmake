# Configure deployment variables

set(PROJECT_QT_PLUGINS_DEST_DIR "Plugins")
set(PROJECT_EXECUTABLE "Drop.exe")
set(PROJECT_SEARCH_PATHS "${QT_BINARY_DIR}")

# Install main executable

install(TARGETS Drop DESTINATION . )

# Install Qt configuration files

install(
    FILES "${CMAKE_CURRENT_LIST_DIR}/qt.conf"
    DESTINATION .
)

# Install required system libraries

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION . )
include(InstallRequiredSystemLibraries)

# Install required Qt plugins

set(
    REQUIRED_QT_PLUGINS
    "iconengines/qsvgicon.dll"
    "platforms/qwindows.dll"
    "styles/qmodernwindowsstyle.dll"
)

foreach(REQUIRED_QT_PLUGIN ${REQUIRED_QT_PLUGINS})
    get_filename_component(REQUIRED_QT_PLUGIN_DIR ${REQUIRED_QT_PLUGIN} PATH)
    install(
        FILES "${QT_PLUGINS_DIR}/${REQUIRED_QT_PLUGIN}"
        DESTINATION "Plugins/${REQUIRED_QT_PLUGIN_DIR}"
    )
endforeach()

# Configure and install bundle fixup script

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/FixupBundle.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake
    @ONLY
)

install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake)

# Configure packaging

set(CPACK_GENERATOR "NSIS")

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")

set(CPACK_PACKAGE_NAME "${PROJECT_TITLE}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_DEVELOPER_TITLE}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_MONOLITHIC_INSTALL ON)

set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

# Uninstall key and name in the list of installed apps are the same for all versions,
# so a new version replaces the previous one instead of being registered next to it
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${PROJECT_TITLE}")

set(CPACK_NSIS_MUI_LANGUAGE "English")
set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_TITLE}")
set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_TITLE}")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/${PROJECT_ICON}")
set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/${PROJECT_ICON}")
set(CPACK_NSIS_INSTALLED_ICON_NAME "${PROJECT_EXECUTABLE}")
set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")

# The installed version is removed silently before installing files, so the uninstall-before-install
# question of CPack is not needed

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/Windows/RemovePreviousInstalls.nsh.in
    ${CMAKE_CURRENT_BINARY_DIR}/RemovePreviousInstalls.nsh
    @ONLY
)

# NSIS doesn't treat "D:/..." as an absolute path, so a native one with escaped backslashes is used
file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/RemovePreviousInstalls.nsh" REMOVE_PREVIOUS_INSTALLS_NSH)
string(REPLACE "\\" "\\\\" REMOVE_PREVIOUS_INSTALLS_NSH "${REMOVE_PREVIOUS_INSTALLS_NSH}")

set(
    CPACK_NSIS_DEFINES "
    !define MUI_FINISHPAGE_RUN \\\"explorer.exe\\\"
    !define MUI_FINISHPAGE_RUN_PARAMETERS \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROJECT_TITLE}.lnk\\\"
    !include \\\"${REMOVE_PREVIOUS_INSTALLS_NSH}\\\""
)

set(
    CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "
    Call RemovePreviousInstalls"
)

# Start menu shortcut is created and removed by CPack hooks, where the start menu folder is known
# (in the uninstaller it's $MUI_TEMP, $STARTMENU_FOLDER is set by the installer page only)

set(
    CPACK_NSIS_CREATE_ICONS_EXTRA "
    CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROJECT_TITLE}.lnk\\\" \\\"$INSTDIR\\\\${PROJECT_EXECUTABLE}\\\""
)

set(
    CPACK_NSIS_DELETE_ICONS_EXTRA "
    Delete \\\"$SMPROGRAMS\\\\$MUI_TEMP\\\\${PROJECT_TITLE}.lnk\\\""
)

# Devices of the local network connect to the application, so Windows Firewall lets it accept
# connections on private networks only, instead of asking on the first start
set(
    CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    nsExec::Exec 'netsh advfirewall firewall delete rule name=\\\"${PROJECT_TITLE}\\\"'
    nsExec::Exec 'netsh advfirewall firewall add rule name=\\\"${PROJECT_TITLE}\\\" dir=in action=allow program=\\\"$INSTDIR\\\\${PROJECT_EXECUTABLE}\\\" profile=private'"
)

set(
    CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    nsExec::Exec 'netsh advfirewall firewall delete rule name=\\\"${PROJECT_TITLE}\\\"'"
)

include(CPack)
