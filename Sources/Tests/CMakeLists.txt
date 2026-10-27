find_package(Qt6 REQUIRED COMPONENTS Test Network HttpServer Widgets)

# Tests compile the application sources they cover directly, so the application target stays untouched
function(add_drop_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp ${ARGN})

    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE DropDefinitions Qt6::Test Qt6::Network Qt6::HttpServer)

    if(WIN32)
        target_compile_options(${TEST_NAME} PRIVATE /utf-8 /nologo)
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_drop_test(
    TestStorage
    ${PROJECT_SOURCE_DIR}/Storage.cpp
    ${PROJECT_SOURCE_DIR}/Storage.h
)

add_drop_test(
    TestLocalNetwork
    ${PROJECT_SOURCE_DIR}/LocalNetwork.cpp
    ${PROJECT_SOURCE_DIR}/LocalNetwork.h
)

add_drop_test(
    TestUploads
    ${PROJECT_SOURCE_DIR}/Storage.cpp
    ${PROJECT_SOURCE_DIR}/Storage.h
    ${PROJECT_SOURCE_DIR}/Uploads.cpp
    ${PROJECT_SOURCE_DIR}/Uploads.h
)

add_drop_test(
    TestWebServer
    ${PROJECT_SOURCE_DIR}/Crypto.cpp
    ${PROJECT_SOURCE_DIR}/Crypto.h
    ${PROJECT_SOURCE_DIR}/Storage.cpp
    ${PROJECT_SOURCE_DIR}/Storage.h
    ${PROJECT_SOURCE_DIR}/Uploads.cpp
    ${PROJECT_SOURCE_DIR}/Uploads.h
    ${PROJECT_SOURCE_DIR}/WebServer.cpp
    ${PROJECT_SOURCE_DIR}/WebServer.h
    RawHttp.h
    TestWebServer.qrc
)

target_link_libraries(TestWebServer PRIVATE Sodium::Sodium)

add_drop_test(
    TestApi
    ${PROJECT_SOURCE_DIR}/Crypto.cpp
    ${PROJECT_SOURCE_DIR}/Crypto.h
    ${PROJECT_SOURCE_DIR}/Storage.cpp
    ${PROJECT_SOURCE_DIR}/Storage.h
    ${PROJECT_SOURCE_DIR}/Uploads.cpp
    ${PROJECT_SOURCE_DIR}/Uploads.h
    ${PROJECT_SOURCE_DIR}/WebServer.cpp
    ${PROJECT_SOURCE_DIR}/WebServer.h
    RawHttp.h
)

target_link_libraries(TestApi PRIVATE Sodium::Sodium)

add_drop_test(
    TestQrCodeWidget
    ${PROJECT_SOURCE_DIR}/QrCodeWidget.cpp
    ${PROJECT_SOURCE_DIR}/QrCodeWidget.h
)

target_link_libraries(TestQrCodeWidget PRIVATE Qt6::Widgets QrCodeGen)

add_drop_test(
    TestCrypto
    ${PROJECT_SOURCE_DIR}/Crypto.cpp
    ${PROJECT_SOURCE_DIR}/Crypto.h
)

target_link_libraries(TestCrypto PRIVATE Sodium::Sodium)
target_compile_definitions(TestCrypto PRIVATE DROP_VECTORS_PATH="${PROJECT_SOURCE_DIR}/../docs/vectors/protocol.json")

add_drop_test(
    TestTranslator
    ${PROJECT_SOURCE_DIR}/Translator.cpp
    ${PROJECT_SOURCE_DIR}/Translator.h
)

target_compile_definitions(TestTranslator PRIVATE DROP_TRANSLATED_SOURCE_PATH="${PROJECT_SOURCE_DIR}/Drop.cpp")

# The server without the window, driven by the end-to-end tests of the web client in Web/e2e

add_executable(
    DropHeadless
    DropHeadless.cpp
    ${PROJECT_SOURCE_DIR}/Crypto.cpp
    ${PROJECT_SOURCE_DIR}/Crypto.h
    ${PROJECT_SOURCE_DIR}/LocalNetwork.cpp
    ${PROJECT_SOURCE_DIR}/LocalNetwork.h
    ${PROJECT_SOURCE_DIR}/Storage.cpp
    ${PROJECT_SOURCE_DIR}/Storage.h
    ${PROJECT_SOURCE_DIR}/Uploads.cpp
    ${PROJECT_SOURCE_DIR}/Uploads.h
    ${PROJECT_SOURCE_DIR}/WebServer.cpp
    ${PROJECT_SOURCE_DIR}/WebServer.h
)

target_include_directories(DropHeadless PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(DropHeadless PRIVATE DropDefinitions Qt6::Network Qt6::HttpServer Sodium::Sodium)

if(WIN32)
    target_compile_options(DropHeadless PRIVATE /utf-8 /nologo)
endif()

# The web client code (Api, Uploader) against the real server, see Web/e2e. Run by node directly
# with the options of "npm run test:e2e": npm is a batch file on Windows, which CTest cannot start.
find_program(NODE_EXECUTABLE NAMES node REQUIRED)

add_test(
    NAME EndToEnd
    COMMAND ${NODE_EXECUTABLE} node_modules/vitest/vitest.mjs run --dir e2e --testTimeout 60000
    WORKING_DIRECTORY ${WEB_SOURCE_DIR}
)

set_tests_properties(EndToEnd PROPERTIES ENVIRONMENT "DROP_HEADLESS=$<TARGET_FILE:DropHeadless>")
