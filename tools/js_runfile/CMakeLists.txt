# JavaScript file runner (single-shot)

add_executable(js_runfile main.cpp)

target_link_libraries(js_runfile PRIVATE
    lithium_core
    lithium_js
    lithium_compiler_options
)

set_target_properties(js_runfile PROPERTIES
    OUTPUT_NAME "lithium-js-run"
)
