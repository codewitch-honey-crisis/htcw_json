cmake_minimum_required(VERSION 3.24)
project(htcw_json VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(htcw_json INTERFACE)
target_link_libraries(htcw_io htcw_bits)

target_include_directories(htcw_json INTERFACE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}"
)
