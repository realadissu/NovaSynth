cmake_minimum_required(VERSION 3.22)

# macOS Deployment Target (must be set before project())
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment target")

project("NovaPlugins" VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

include("${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/NovaPluginConfig.cmake")

# Shared Libraries
add_subdirectory(libs/nova_core_dsp)
add_subdirectory(libs/nova_core_plugin)
add_subdirectory(libs/nova_core_ui)

# Plugins
add_subdirectory(plugins/nova_space)
add_subdirectory(plugins/nova_synth)

# Unit & Realtime-Safety Tests
add_subdirectory(tests)

# Evaluation Tools
add_subdirectory(tools)
