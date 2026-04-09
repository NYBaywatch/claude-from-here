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

# Disable auto-generated linker manifest: we embed our own via ClaudeFromHere.rc (ID 2 RT_MANIFEST)
if(MSVC)
    target_link_options(ClaudeFromHere PRIVATE /MANIFEST:NO)
endif()

# Minimal stub executable required by AppxManifest Application Executable attribute.
# The COM surrogate DLL is loaded by dllhost.exe, not this stub; the stub is never invoked.
add_executable(ClaudeFromHereStub WIN32 src/stub_main.cpp)
set_target_properties(ClaudeFromHereStub PROPERTIES
    OUTPUT_NAME "ClaudeFromHere"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/build/Debug"
)
