# Layout module - Box model and layout algorithms

lithium_add_module(layout
    SOURCES
        src/box.cpp
        src/layout_context.cpp
        src/block_layout.cpp
        src/inline_layout.cpp
    HEADERS
        include/lithium/layout/box.hpp
        include/lithium/layout/layout_context.hpp
        include/lithium/layout/block_layout.hpp
        include/lithium/layout/inline_layout.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_dom
        lithium_css
        lithium_beryl
)
