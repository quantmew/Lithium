# Integration tests for Lithium HTML parser

option(LITHIUM_ENABLE_INTEGRATION_TESTS "Enable integration tests" OFF)

if(LITHIUM_ENABLE_INTEGRATION_TESTS)
    # Simple test programs for testing components individually
    add_executable(tokenizer_test tokenizer_test.cpp)
    target_link_libraries(tokenizer_test lithium_html lithium_core lithium_dom)

    # Add to tests
    add_test(NAME tokenizer_test COMMAND tokenizer_test)
endif()
