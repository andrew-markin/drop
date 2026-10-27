# Build RPM for Fedora/RHEL/AlmaLinux

set(RPM_PACKAGE_NAME ${PACKAGE_NAME})
set(RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
set(RPM_PACKAGE_RELEASE "1")
# The application, bundled Qt, libsodium linked in statically, and the libraries of the web client
set(RPM_PACKAGE_LICENSE "GPL-3.0-only AND (LGPL-3.0-only OR GPL-3.0-only WITH Qt-GPL-exception-1.0) AND ISC AND MIT")
set(RPM_PACKAGE_GROUP "Productivity/Other")
set(RPM_PACKAGE_SUMMARY ${PROJECT_DESCRIPTION})
set(RPM_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(RPM_PACKAGER ${PROJECT_DEVELOPER})
set(RPM_PACKAGE_URL ${PROJECT_HOME_URL})
set(RPM_PACKAGE_ARCH "x86_64")

# Dependencies are detected by rpmbuild from the binaries automatically

# Configure and install package building script

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/RPM/Build.in"
    "${PROJECT_BINARY_DIR}/RPMBuild"
    @ONLY
)

file(
    CHMOD "${PROJECT_BINARY_DIR}/RPMBuild"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)

# Add custom target for RPM-packaging

add_custom_target(
    "deploy_rpm"
    COMMAND "${PROJECT_BINARY_DIR}/RPMBuild" "${DEPLOY_PACKAGE_DIR}"
    DEPENDS "deploy_package"
)
