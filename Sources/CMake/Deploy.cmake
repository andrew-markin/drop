set(DEPLOY_PACKAGE_DIR "${PROJECT_BINARY_DIR}/Package")

add_custom_target(
    "deploy_package"
    COMMAND "${CMAKE_COMMAND}"
    "-DCMAKE_INSTALL_LOCAL_ONLY=1"
    "-DCMAKE_INSTALL_DO_STRIP=1"
    "-DCMAKE_INSTALL_PREFIX=${DEPLOY_PACKAGE_DIR}"
    "-P" "${CMAKE_BINARY_DIR}/cmake_install.cmake"
)

if(WIN32)
    include(DeployWindows)
elseif(LINUX)
    include(DeployLinux)
endif()
