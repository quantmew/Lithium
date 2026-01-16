# DOM module - Document Object Model implementation

lithium_add_module(dom
    SOURCES
        src/node.cpp
        src/element.cpp
        src/document.cpp
        src/text.cpp
    HEADERS
        include/lithium/dom/node.hpp
        include/lithium/dom/element.hpp
        include/lithium/dom/document.hpp
        include/lithium/dom/text.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)
