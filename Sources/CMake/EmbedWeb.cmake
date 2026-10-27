# Builds the web client with npm and compiles it into the target resources under ":/Web"

find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)

set(WEB_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../Web")
get_filename_component(WEB_SOURCE_DIR "${WEB_SOURCE_DIR}" ABSOLUTE)

function(embed_web TARGET)
    set(WEB_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Web")
    set(WEB_DIST_DIR "${WEB_BINARY_DIR}/dist")
    set(WEB_QRC_FILE "${WEB_BINARY_DIR}/Web.qrc")
    set(WEB_RESOURCES_FILE "${WEB_BINARY_DIR}/qrc_Web.cpp")
    set(NPM_CI_STAMP_FILE "${WEB_BINARY_DIR}/npm-ci.stamp")

    file(MAKE_DIRECTORY ${WEB_BINARY_DIR})

    file(
        GLOB_RECURSE WEB_INPUTS CONFIGURE_DEPENDS
        "${WEB_SOURCE_DIR}/src/*"
        "${WEB_SOURCE_DIR}/public/*"
    )

    list(
        APPEND WEB_INPUTS
        "${WEB_SOURCE_DIR}/index.html"
        "${WEB_SOURCE_DIR}/package.json"
        "${WEB_SOURCE_DIR}/vite.config.js"
    )

    # Dependencies are installed again only when the lock file changes
    add_custom_command(
        OUTPUT ${NPM_CI_STAMP_FILE}
        COMMAND ${NPM_EXECUTABLE} ci --no-audit --no-fund
        COMMAND ${CMAKE_COMMAND} -E touch ${NPM_CI_STAMP_FILE}
        DEPENDS "${WEB_SOURCE_DIR}/package-lock.json"
        WORKING_DIRECTORY ${WEB_SOURCE_DIR}
        COMMENT "Installing web client dependencies"
        VERBATIM
    )

    # Bundle names carry content hashes, so the resource list is made after each build
    add_custom_command(
        OUTPUT ${WEB_RESOURCES_FILE}
        COMMAND ${NPM_EXECUTABLE} run build -- --outDir ${WEB_DIST_DIR} --emptyOutDir --logLevel warn
        COMMAND ${CMAKE_COMMAND} -DDIST_DIR=${WEB_DIST_DIR} -DQRC_FILE=${WEB_QRC_FILE}
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateWebQrc.cmake
        COMMAND Qt6::rcc --name Web --output ${WEB_RESOURCES_FILE} ${WEB_QRC_FILE}
        DEPENDS ${NPM_CI_STAMP_FILE} ${WEB_INPUTS} ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateWebQrc.cmake
        WORKING_DIRECTORY ${WEB_SOURCE_DIR}
        COMMENT "Building web client"
        VERBATIM
    )

    set_source_files_properties(${WEB_RESOURCES_FILE} PROPERTIES SKIP_AUTOGEN ON)
    target_sources(${TARGET} PRIVATE ${WEB_RESOURCES_FILE})
endfunction()
