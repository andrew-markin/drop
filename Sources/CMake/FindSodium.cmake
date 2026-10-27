# Finds libsodium and provides the Sodium::Sodium target
#
# vcpkg (Windows) installs a CMake package, Linux distributions install a pkg-config file

find_package(unofficial-sodium CONFIG QUIET)

if(TARGET unofficial-sodium::sodium)
    add_library(Sodium::Sodium ALIAS unofficial-sodium::sodium)
    set(Sodium_FOUND TRUE)
    return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM IMPORTED_TARGET libsodium>=1.0.18)

if(SODIUM_FOUND)
    add_library(Sodium::Sodium ALIAS PkgConfig::SODIUM)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium REQUIRED_VARS SODIUM_FOUND VERSION_VAR SODIUM_VERSION)
