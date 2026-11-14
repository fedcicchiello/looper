cmake_minimum_required(VERSION 3.22)

project(Looper VERSION 0.1.0 LANGUAGES C CXX) # JUCE compiles a few C files

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOOPER_BUILD_TESTS "Build the unit tests" ON)
option(LOOPER_BUILD_PLUGIN "Build the JUCE plugin (VST3) and the standalone app" ON)

add_subdirectory(src/core)

if(LOOPER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOOPER_BUILD_PLUGIN)
    add_subdirectory(src/plugin)
endif()
