# Platform abstraction module

set(PLATFORM_SOURCES
    src/event.cpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND PLATFORM_SOURCES
        src/windows/window_win32.cpp
        src/windows/graphics_context_win32.cpp
    )
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES
        src/macos/window_cocoa.mm
        src/macos/graphics_context_cocoa.mm
    )
    enable_language(OBJCXX)
elseif(UNIX)
    list(APPEND PLATFORM_SOURCES
        src/linux/window_x11.cpp
        src/linux/graphics_context_x11.cpp
    )
    find_package(X11 REQUIRED)
endif()

lithium_add_module(platform
    SOURCES
        ${PLATFORM_SOURCES}
    HEADERS
        include/lithium/platform/window.hpp
        include/lithium/platform/graphics_context.hpp
        include/lithium/platform/event.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(lithium_platform PRIVATE
        user32
        gdi32
        dwmapi
    )
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZ_LIBRARY QuartzCore REQUIRED)
    target_link_libraries(lithium_platform PRIVATE
        ${COCOA_LIBRARY}
        ${QUARTZ_LIBRARY}
    )
elseif(UNIX)
    target_link_libraries(lithium_platform PRIVATE
        ${X11_LIBRARIES}
    )
    target_include_directories(lithium_platform PRIVATE
        ${X11_INCLUDE_DIR}
    )
endif()
