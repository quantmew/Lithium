# JavaScript module - JavaScript engine (partial ES6)

lithium_add_module(js
    SOURCES
        src/lexer.cpp
        src/parser.cpp
        src/ast.cpp
        src/bytecode.cpp
        src/compiler.cpp
        src/vm.cpp
        src/value.cpp
        src/object.cpp
    HEADERS
        include/lithium/js/lexer.hpp
        include/lithium/js/parser.hpp
        include/lithium/js/ast.hpp
        include/lithium/js/bytecode.hpp
        include/lithium/js/compiler.hpp
        include/lithium/js/vm.hpp
        include/lithium/js/value.hpp
        include/lithium/js/object.hpp
        include/lithium/js/diagnostic.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)
