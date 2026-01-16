# Integration tests

add_executable(test_integration
    test_html_to_render.cpp
)

target_link_libraries(test_integration PRIVATE
    lithium_core
    lithium_dom
    lithium_html
    lithium_css
    lithium_layout
    lithium_render
    lithium_platform
    GTest::gtest
    GTest::gtest_main
    lithium_compiler_options
)

include(GoogleTest)
gtest_discover_tests(test_integration)
