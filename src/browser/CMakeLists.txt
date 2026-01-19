# Browser module - Main application

add_executable(lithium_browser
    main.cpp
    src/engine.cpp
    src/tab.cpp
    src/ui.cpp
)

target_include_directories(lithium_browser PRIVATE
    include
)

target_link_libraries(lithium_browser PRIVATE
    lithium_core
    lithium_platform
    lithium_dom
    lithium_html
    lithium_css
    lithium_js
    lithium_layout
    lithium_beryl
    lithium_mica
    lithium_bindings
    lithium_network
    lithium_compiler_options
)

set_target_properties(lithium_browser PROPERTIES
    OUTPUT_NAME "lithium"
)
