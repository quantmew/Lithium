# CompilerOptions.cmake
# Sets up compiler warnings and options for the project

# Create an interface library for compiler options
add_library(lithium_compiler_options INTERFACE)

# Detect compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(lithium_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Wno-unused-parameter
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    # Additional warnings for GCC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(lithium_compiler_options INTERFACE
            -Wlogical-op
            -Wduplicated-cond
            -Wduplicated-branches
        )
    endif()

    # Additional warnings for Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(lithium_compiler_options INTERFACE
            -Wshadow
            -Wno-gnu-anonymous-struct
            -Wno-nested-anon-types
        )
    endif()

    # Debug-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(lithium_compiler_options INTERFACE
            -g3
            -fno-omit-frame-pointer
        )

        # Sanitizers (optional)
        if(LITHIUM_USE_SANITIZERS)
            target_compile_options(lithium_compiler_options INTERFACE
                -fsanitize=address,undefined
            )
            target_link_options(lithium_compiler_options INTERFACE
                -fsanitize=address,undefined
            )
        endif()
    endif()

    # Release-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(lithium_compiler_options INTERFACE
            -O3
            -DNDEBUG
        )
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lithium_compiler_options INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
        /EHsc
    )

    # Debug-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(lithium_compiler_options INTERFACE
            /Zi
            /Od
            /RTC1
        )
    endif()

    # Release-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(lithium_compiler_options INTERFACE
            /O2
            /DNDEBUG
        )
    endif()
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(lithium_compiler_options INTERFACE
        LITHIUM_PLATFORM_WINDOWS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(APPLE)
    target_compile_definitions(lithium_compiler_options INTERFACE
        LITHIUM_PLATFORM_MACOS
    )
elseif(UNIX)
    target_compile_definitions(lithium_compiler_options INTERFACE
        LITHIUM_PLATFORM_LINUX
    )
endif()
