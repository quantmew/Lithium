cmake_minimum_required(VERSION 3.16)
project(Lithium
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "A lightweight browser engine implemented from scratch"
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(LITHIUM_BUILD_TESTS "Build unit tests" ON)
option(LITHIUM_BUILD_TOOLS "Build development tools" ON)
option(LITHIUM_USE_SANITIZERS "Enable sanitizers in debug builds" OFF)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(AddModule)

# Conan integration (if using conan)
if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    include("${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
endif()

# Find external dependencies
find_package(Freetype QUIET)
find_package(CURL QUIET)

if(LITHIUM_BUILD_TESTS)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, will use FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# Core modules (order matters for dependencies)
add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/dom)
add_subdirectory(src/html)
add_subdirectory(src/css)
add_subdirectory(src/js)
# add_subdirectory(src/text)  # Temporarily disabled - needs fixes
# add_subdirectory(src/layout)  # Temporarily disabled - depends on text module
# add_subdirectory(src/render)  # Temporarily disabled - depends on text module
# add_subdirectory(src/bindings)  # Temporarily disabled - depends on js module
# add_subdirectory(src/network)  # Temporarily disabled - needs String API updates
# add_subdirectory(src/browser)  # Temporarily disabled - depends on other modules

# Tests
if(LITHIUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Development tools
if(LITHIUM_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Installation
include(GNUInstallDirs)
# install(TARGETS lithium_browser
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

# Print configuration summary
message(STATUS "")
message(STATUS "Lithium Browser Engine v${PROJECT_VERSION}")
message(STATUS "================================")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests:       ${LITHIUM_BUILD_TESTS}")
message(STATUS "Build tools:       ${LITHIUM_BUILD_TOOLS}")
message(STATUS "Use sanitizers:    ${LITHIUM_USE_SANITIZERS}")
message(STATUS "")
