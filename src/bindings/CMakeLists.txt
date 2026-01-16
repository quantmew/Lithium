# Bindings module - JavaScript-DOM bindings

lithium_add_module(bindings
    SOURCES
        src/dom_bindings.cpp
        src/window_binding.cpp
        src/document_binding.cpp
        src/element_binding.cpp
    HEADERS
        include/lithium/bindings/dom_bindings.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
        lithium_dom
        lithium_js
)
