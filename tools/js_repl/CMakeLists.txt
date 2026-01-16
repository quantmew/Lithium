# JavaScript REPL tool

add_executable(js_repl main.cpp)

target_link_libraries(js_repl PRIVATE
    lithium_core
    lithium_js
    lithium_compiler_options
)

set_target_properties(js_repl PROPERTIES
    OUTPUT_NAME "lithium-js"
)
