cmake_minimum_required(VERSION 3.12)
project(nfc_userconf VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_SOURCE_DIR src)
set(OUTPUT_DIR ../bin)

set(RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})

add_executable(${PROJECT_NAME} src/nfc_userconf.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC ../linux-pam/include)

file(GLOB GLOB_SOURCES src/*.cpp)
list(REMOVE_ITEM GLOB_SOURCES src/nfc_userconf.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${GLOB_SOURCES})

target_sources(${PROJECT_NAME} PRIVATE  ../linux-pam/src/Comms.cpp ../linux-pam/src/base64.cpp ../linux-pam/src/format.cpp ../linux-pam/src/OSSLUtils.cpp)

configure_file(include/cmake_version.h.in include/cmake_version.h)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector")

#https://stackoverflow.com/questions/17707044/getting-cmake-to-give-an-error-warning-about-unreferenced-symbols
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_link_libraries(${PROJECT_NAME} ssl crypto sqlite3)