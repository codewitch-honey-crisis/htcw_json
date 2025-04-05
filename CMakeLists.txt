cmake_minimum_required(VERSION 3.24)
project(htcw_json VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

include(FetchContent)
FetchContent_Declare(htcw_io
    GIT_REPOSITORY https://github.com/codewitch-honey-crisis/htcw_io.git
    GIT_TAG 1.4.0
    GIT_SHALLOW ON
    FIND_PACKAGE_ARGS 1.4.0
)

FetchContent_MakeAvailable(htcw_io)

add_library(htcw_json INTERFACE)
target_link_libraries(htcw_io htcw_bits)

target_include_directories(htcw_json INTERFACE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}"
)
