# Development tools

add_subdirectory(html_parser_cli)
add_subdirectory(css_parser_cli)

if(TARGET lithium_js)
    add_subdirectory(js_repl)
    add_subdirectory(js_runfile)
endif()
