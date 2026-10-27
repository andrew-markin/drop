add_library(QrCodeGen STATIC qrcodegen.cpp qrcodegen.hpp)
target_include_directories(QrCodeGen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_compile_options(QrCodeGen PRIVATE /utf-8 /nologo)
endif()

# Plain C++ without Qt, nothing for moc to process
set_target_properties(QrCodeGen PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
