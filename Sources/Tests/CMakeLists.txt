find_package(Qt6 REQUIRED COMPONENTS Test Network)

# Tests compile the application sources they cover directly, so the application target stays untouched
function(add_drop_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp ${ARGN})

    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE DropDefinitions Qt6::Test Qt6::Network)

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
