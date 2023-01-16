cmake_minimum_required(VERSION 3.12)
project(pam_nfc VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_SOURCE_DIR src)
set(OUTPUT_DIR ../bin)

set(LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})

add_library(pam_nfc SHARED src/pam_nfc.cpp)
target_include_directories(pam_nfc PUBLIC include)

file(GLOB GLOB_SOURCES src/*.cpp)
list(REMOVE_ITEM GLOB_SOURCES src/pam_nfc.cpp)
target_sources(pam_nfc PRIVATE ${GLOB_SOURCES})

configure_file(include/cmake_version.h.in include/cmake_version.h)
set_target_properties(pam_nfc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set_target_properties(pam_nfc PROPERTIES PREFIX "")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector")

#https://stackoverflow.com/questions/17707044/getting-cmake-to-give-an-error-warning-about-unreferenced-symbols
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_link_libraries(pam_nfc pam ssl crypto sqlite3)