# Core module - Foundation types and utilities

lithium_add_module(core
    SOURCES
        src/logger.cpp
        src/string.cpp
    HEADERS
        include/lithium/core/types.hpp
        include/lithium/core/string.hpp
        include/lithium/core/memory.hpp
        include/lithium/core/logger.hpp
)
