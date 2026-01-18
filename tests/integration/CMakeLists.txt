# Integration tests for Lithium HTML parser

option(LITHIUM_ENABLE_INTEGRATION_TESTS "Enable integration tests" OFF)

if(LITHIUM_ENABLE_INTEGRATION_TESTS)
    # Simple test programs for testing components individually
    add_executable(tokenizer_test tokenizer_test.cpp)
    target_link_libraries(tokenizer_test lithium_html lithium_core lithium_dom)

    # Add to tests
    add_test(NAME tokenizer_test COMMAND tokenizer_test)

    # Text rendering test
    add_executable(text_render_test ../text_render_test.cpp)
    target_link_libraries(text_render_test lithium_platform lithium_core)

    # Direct2D text test
    add_executable(d2d_text_test ../d2d_text_test.cpp)
    target_link_libraries(d2d_text_test lithium_platform lithium_core)

    # Set output directory
    set_target_properties(text_render_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
    )
    set_target_properties(d2d_text_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
    )
endif()
