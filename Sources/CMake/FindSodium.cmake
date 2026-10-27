# Finds libsodium and provides the Sodium::Sodium target
#
# vcpkg (Windows) installs a CMake package, Linux distributions install a pkg-config file

find_package(unofficial-sodium CONFIG QUIET)

if(TARGET unofficial-sodium::sodium)
    get_target_property(SODIUM_TARGET_TYPE unofficial-sodium::sodium TYPE)

    # Headers of a static libsodium must not declare its functions as imported from a DLL
    if(SODIUM_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
        set_property(TARGET unofficial-sodium::sodium APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SODIUM_STATIC)
    endif()

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
