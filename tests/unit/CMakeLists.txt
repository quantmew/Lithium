# Unit tests for all modules

# Core module tests
lithium_add_module_tests(core
    SOURCES
        core/test_types.cpp
        core/test_string.cpp
)

# HTML module tests
lithium_add_module_tests(html
    SOURCES
        html/test_tokenizer.cpp
        html/test_parser.cpp
    DEPENDENCIES
        lithium_dom
)

# CSS module tests
lithium_add_module_tests(css
    SOURCES
        css/test_tokenizer.cpp
        css/test_parser.cpp
        css/test_selector.cpp
    DEPENDENCIES
        lithium_dom
)

# JavaScript module tests
lithium_add_module_tests(js
    SOURCES
        js/test_lexer.cpp
        js/test_parser.cpp
        js/test_vm.cpp
)

# Layout module tests
lithium_add_module_tests(layout
    SOURCES
        layout/test_box.cpp
        layout/test_block_layout.cpp
    DEPENDENCIES
        lithium_dom
        lithium_css
        lithium_text
)

# Render module tests
lithium_add_module_tests(render
    SOURCES
        render/test_display_list.cpp
    DEPENDENCIES
        lithium_layout
        lithium_platform
)
