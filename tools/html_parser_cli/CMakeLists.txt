# HTML Parser CLI tool

add_executable(html_parser_cli main.cpp)

target_link_libraries(html_parser_cli PRIVATE
    lithium_core
    lithium_dom
    lithium_html
    lithium_compiler_options
)

set_target_properties(html_parser_cli PROPERTIES
    OUTPUT_NAME "lithium-html"
)
