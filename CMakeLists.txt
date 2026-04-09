cmake_minimum_required(VERSION 3.28)
project(ClaudeFromHere LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(ClaudeFromHere SHARED
    src/dllmain.cpp
    src/ClaudeFromHere.cpp
    src/ClaudeFromHere.def
    src/ClaudeFromHere.rc
)

target_link_libraries(ClaudeFromHere PRIVATE
    shlwapi
    shell32
    ole32
)

set_target_properties(ClaudeFromHere PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)
