# HTML module - HTML parser implementation

lithium_add_module(html
    SOURCES
        src/tokenizer/tokenizer_core.cpp
        src/tokenizer/tokenizer_state_data.cpp
        src/tokenizer/tokenizer_state_tag.cpp
        src/tokenizer/tokenizer_state_comment_doctype.cpp
        src/tokenizer/tokenizer_state_charref.cpp
        src/tokenizer/tokenizer_tokens.cpp
        src/parser.cpp
        src/tree_builder/tree_builder_core.cpp
        src/tree_builder/tree_builder_insertion_head.cpp
        src/tree_builder/tree_builder_insertion_body.cpp
        src/tree_builder/tree_builder_insertion_table.cpp
        src/tree_builder/constants.hpp
    HEADERS
        include/lithium/html/tokenizer.hpp
        include/lithium/html/parser.hpp
        include/lithium/html/tree_builder.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_dom
)
