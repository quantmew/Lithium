# Text module - Text shaping and rendering

lithium_add_module(text
    SOURCES
        src/font.cpp
        src/shaper.cpp
        src/line_breaker.cpp
    HEADERS
        include/lithium/text/font.hpp
        include/lithium/text/shaper.hpp
        include/lithium/text/line_breaker.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)

# Optional FreeType integration
if(Freetype_FOUND)
    target_link_libraries(lithium_text PRIVATE Freetype::Freetype)
    target_compile_definitions(lithium_text PRIVATE LITHIUM_USE_FREETYPE)
endif()
