# Unit tests for all modules

# Core module tests
lithium_add_module_tests(core
    SOURCES
        core/test_types.cpp
        core/test_string.cpp
)

# DOM module tests
set(DOM_TEST_SOURCES
    dom/test_inner_html.cpp
)
set(DOM_TEST_DEPENDENCIES
    lithium_html
)

if(TARGET lithium_bindings)
    list(APPEND DOM_TEST_SOURCES dom/test_dom_bindings.cpp)
    list(APPEND DOM_TEST_DEPENDENCIES lithium_bindings lithium_js)
endif()

lithium_add_module_tests(dom
    SOURCES
        ${DOM_TEST_SOURCES}
    DEPENDENCIES
        ${DOM_TEST_DEPENDENCIES}
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
if(TARGET lithium_js)
    lithium_add_module_tests(js
        SOURCES
            js/test_lexer.cpp
            js/test_parser.cpp
            js/test_vm.cpp
    )
endif()

# Layout module tests
if(TARGET lithium_layout)
    lithium_add_module_tests(layout
        SOURCES
            layout/test_box.cpp
            layout/test_block_layout.cpp
        DEPENDENCIES
            lithium_dom
            lithium_css
            lithium_text
    )
endif()

# Render module tests
if(TARGET lithium_render)
    lithium_add_module_tests(render
        SOURCES
            render/test_display_list.cpp
        DEPENDENCIES
            lithium_layout
            lithium_platform
    )
endif()

# Platform module tests
if(TARGET lithium_platform)
    lithium_add_module_tests(platform
        SOURCES
            ../platform/test_graphics_backends.cpp
        DEPENDENCIES
            lithium_core
    )
endif()
