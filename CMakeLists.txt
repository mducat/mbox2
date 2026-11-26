cmake_minimum_required(VERSION 3.22)

project(MusicBox2
    VERSION 0.1.0
    DESCRIPTION "Music theory & geometric composition aid (VST3 / Standalone)"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MBOX_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
option(MBOX_BUILD_TESTS  "Build the music theory unit tests" ON)
option(MBOX_BUILD_CLAP   "Also build a CLAP version (clap-juce-extensions)" OFF)

# Static MSVC runtime so the VST3 does not depend on the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Pure C++ music theory library: no JUCE, no UI.
add_subdirectory(core)

if(MBOX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
endif()

if(MBOX_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
