# CSS module - CSS parser and style resolution

lithium_add_module(css
    SOURCES
        src/tokenizer.cpp
        src/parser.cpp
        src/selector.cpp
        src/value.cpp
        src/style_resolver.cpp
    HEADERS
        include/lithium/css/tokenizer.hpp
        include/lithium/css/parser.hpp
        include/lithium/css/selector.hpp
        include/lithium/css/value.hpp
        include/lithium/css/style_resolver.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_dom
)
