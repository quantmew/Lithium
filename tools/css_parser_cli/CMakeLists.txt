# CSS Parser CLI tool

add_executable(css_parser_cli main.cpp)

target_link_libraries(css_parser_cli PRIVATE
    lithium_core
    lithium_dom
    lithium_css
    lithium_compiler_options
)

set_target_properties(css_parser_cli PROPERTIES
    OUTPUT_NAME "lithium-css"
)
