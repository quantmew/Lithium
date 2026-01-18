# Platform abstraction module

set(PLATFORM_SOURCES
    src/event.cpp
    src/graphics_context.cpp
    src/graphics_factory.cpp
)

# Graphics backend options
option(LITHIUM_ENABLE_OPENGL "Enable OpenGL backend" ON)
option(LITHIUM_ENABLE_DIRECT2D "Enable Direct2D backend (Windows only)" ON)

# Platform-specific sources
if(WIN32)
    list(APPEND PLATFORM_SOURCES
        src/windows/window_win32.cpp
        src/windows/graphics_context_win32.cpp
    )

    # Direct2D backend
    if(LITHIUM_ENABLE_DIRECT2D)
        list(APPEND PLATFORM_SOURCES
            src/direct2d/d2d_graphics_context.cpp
            src/direct2d/d2d_render_state.cpp
            src/direct2d/d2d_text_renderer.cpp
            src/direct2d/d2d_texture_cache.cpp
        )
        set(HAS_DIRECT2D TRUE)
    endif()

    # OpenGL backend
    if(LITHIUM_ENABLE_OPENGL)
        list(APPEND PLATFORM_SOURCES
            src/opengl/opengl_graphics_context.cpp
            src/opengl/opengl_shader_manager.cpp
            src/opengl/opengl_render_state.cpp
            src/opengl/opengl_texture_cache.cpp
            src/opengl/opengl_utils.cpp
        )
        set(HAS_OPENGL TRUE)
    endif()

elseif(APPLE)
    list(APPEND PLATFORM_SOURCES
        src/macos/window_cocoa.mm
        src/macos/graphics_context_cocoa.mm
    )
    enable_language(OBJCXX)

    # OpenGL backend for macOS
    if(LITHIUM_ENABLE_OPENGL)
        list(APPEND PLATFORM_SOURCES
            src/opengl/opengl_graphics_context.cpp
            src/opengl/opengl_shader_manager.cpp
            src/opengl/opengl_render_state.cpp
            src/opengl/opengl_texture_cache.cpp
            src/opengl/opengl_utils.cpp
        )
        set(HAS_OPENGL TRUE)
    endif()

elseif(UNIX)
    list(APPEND PLATFORM_SOURCES
        src/linux/window_x11.cpp
        src/linux/graphics_context_x11.cpp
    )
    find_package(X11 REQUIRED)

    # OpenGL backend for Linux
    if(LITHIUM_ENABLE_OPENGL)
        list(APPEND PLATFORM_SOURCES
            src/opengl/opengl_graphics_context.cpp
            src/opengl/opengl_shader_manager.cpp
            src/opengl/opengl_render_state.cpp
            src/opengl/opengl_texture_cache.cpp
            src/opengl/opengl_utils.cpp
        )
        set(HAS_OPENGL TRUE)
    endif()
endif()

# Shared utilities (if any HW backend enabled)
if(LITHIUM_ENABLE_OPENGL OR LITHIUM_ENABLE_DIRECT2D)
    list(APPEND PLATFORM_SOURCES
        src/shared/texture_atlas.cpp
        src/shared/glyph_cache.cpp
        src/shared/batch_renderer.cpp
    )
    set(HAS_HARDWARE_ACCELERATION TRUE)
endif()

lithium_add_module(platform
    SOURCES
        ${PLATFORM_SOURCES}
    HEADERS
        include/lithium/platform/window.hpp
        include/lithium/platform/graphics_context.hpp
        include/lithium/platform/graphics_config.hpp
        include/lithium/platform/graphics_backend.hpp
        include/lithium/platform/event.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)

# Apply compile definitions after target is created
if(HAS_DIRECT2D)
    target_compile_definitions(lithium_platform PRIVATE LITHIUM_HAS_DIRECT2D)
endif()

if(HAS_OPENGL)
    target_compile_definitions(lithium_platform PRIVATE LITHIUM_HAS_OPENGL)
endif()

if(HAS_HARDWARE_ACCELERATION)
    target_compile_definitions(lithium_platform PRIVATE LITHIUM_HARDWARE_ACCELERATED)
endif()

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(lithium_platform PRIVATE
        user32
        gdi32
        dwmapi
    )

    # Direct2D libraries
    if(LITHIUM_ENABLE_DIRECT2D)
        target_link_libraries(lithium_platform PRIVATE
            d2d1
            d3d11
            dxgi
            dwrite
        )
    endif()

    # OpenGL libraries (Windows has built-in OpenGL)
    if(LITHIUM_ENABLE_OPENGL)
        target_link_libraries(lithium_platform PRIVATE
            opengl32
        )
    endif()

elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZ_LIBRARY QuartzCore REQUIRED)
    target_link_libraries(lithium_platform PRIVATE
        ${COCOA_LIBRARY}
        ${QUARTZ_LIBRARY}
    )

    # OpenGL framework on macOS
    if(LITHIUM_ENABLE_OPENGL)
        find_library(OPENGL_LIBRARY OpenGL REQUIRED)
        target_link_libraries(lithium_platform PRIVATE
            ${OPENGL_LIBRARY}
        )
    endif()

elseif(UNIX)
    target_link_libraries(lithium_platform PRIVATE
        ${X11_LIBRARIES}
    )
    target_include_directories(lithium_platform PRIVATE
        ${X11_INCLUDE_DIR}
    )

    # OpenGL libraries on Linux
    if(LITHIUM_ENABLE_OPENGL)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GL REQUIRED gl)
        target_link_libraries(lithium_platform PRIVATE
            ${GL_LIBRARIES}
        )
        target_include_directories(lithium_platform PRIVATE
            ${GL_INCLUDE_DIRS}
        )
        target_compile_options(lithium_platform PRIVATE
            ${GL_CFLAGS_OTHER}
        )
    endif()

    # FreeType for text rendering on Linux
    find_package(Freetype REQUIRED)
    target_link_libraries(lithium_platform PRIVATE
        Freetype::Freetype
    )
    target_include_directories(lithium_platform PRIVATE
        ${FREETYPE_INCLUDE_DIRS}
    )
    target_compile_definitions(lithium_platform PRIVATE LITHIUM_HAS_FREETYPE)
endif()
