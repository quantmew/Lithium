# Render module - Rendering pipeline

lithium_add_module(render
    SOURCES
        src/display_list.cpp
        src/paint_context.cpp
        src/compositor.cpp
    HEADERS
        include/lithium/render/display_list.hpp
        include/lithium/render/paint_context.hpp
        include/lithium/render/compositor.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_platform
        lithium_layout
)
