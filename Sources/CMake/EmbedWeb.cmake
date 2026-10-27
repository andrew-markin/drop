# Builds the web client with npm and compiles it into the target resources under ":/Web"

# IDEs started from the desktop don't get the PATH of shells, where version managers like nvm
# put Node.js, so its usual places are searched as well, the newest version first
file(GLOB NVM_NODE_BIN_DIRS "$ENV{HOME}/.nvm/versions/node/*/bin")
list(SORT NVM_NODE_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)

find_program(NODE_EXECUTABLE NAMES node HINTS "$ENV{NVM_BIN}" PATHS ${NVM_NODE_BIN_DIRS} REQUIRED)

# npm and Node.js come together, the npm found next to Node.js is the one that matches it
get_filename_component(NODE_BIN_DIR "${NODE_EXECUTABLE}" DIRECTORY)
find_program(NPM_EXECUTABLE NAMES npm.cmd npm HINTS "${NODE_BIN_DIR}" NO_DEFAULT_PATH)
find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)

# npm and the tools it runs start "node" by name, so it has to be in PATH while they run
if(WIN32)
    set(NPM_COMMAND ${NPM_EXECUTABLE})
else()
    set(NPM_COMMAND ${CMAKE_COMMAND} -E env "PATH=${NODE_BIN_DIR}:$ENV{PATH}" ${NPM_EXECUTABLE})
endif()

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
        COMMAND ${NPM_COMMAND} ci --no-audit --no-fund
        COMMAND ${CMAKE_COMMAND} -E touch ${NPM_CI_STAMP_FILE}
        DEPENDS "${WEB_SOURCE_DIR}/package-lock.json"
        WORKING_DIRECTORY ${WEB_SOURCE_DIR}
        COMMENT "Installing web client dependencies"
        VERBATIM
    )

    # Bundle names carry content hashes, so the resource list is made after each build
    add_custom_command(
        OUTPUT ${WEB_RESOURCES_FILE}
        COMMAND ${NPM_COMMAND} run build -- --outDir ${WEB_DIST_DIR} --emptyOutDir --logLevel warn
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
