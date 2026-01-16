# HTML module - HTML parser implementation

lithium_add_module(html
    SOURCES
        src/tokenizer.cpp
        src/parser.cpp
        src/tree_builder.cpp
    HEADERS
        include/lithium/html/tokenizer.hpp
        include/lithium/html/parser.hpp
        include/lithium/html/tree_builder.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_dom
)
