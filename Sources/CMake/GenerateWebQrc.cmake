cmake_minimum_required(VERSION 3.20)

# Lists every file of the web client build in a resource collection file under the "/Web" prefix
#
# Variables:
#   DIST_DIR - web client build directory
#   QRC_FILE - resource collection file to write

file(GLOB_RECURSE WEB_FILES RELATIVE "${DIST_DIR}" "${DIST_DIR}/*")
list(SORT WEB_FILES)

if(NOT "index.html" IN_LIST WEB_FILES)
    message(FATAL_ERROR "Web client build has no index.html in ${DIST_DIR}")
endif()

set(QRC_CONTENT "<RCC>\n    <qresource prefix=\"/Web\">\n")

foreach(WEB_FILE IN LISTS WEB_FILES)
    string(APPEND QRC_CONTENT "        <file alias=\"${WEB_FILE}\">${DIST_DIR}/${WEB_FILE}</file>\n")
endforeach()

string(APPEND QRC_CONTENT "    </qresource>\n</RCC>\n")

file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")
