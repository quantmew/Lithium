# Beryl Text Engine
add_library(lithium_beryl INTERFACE)
add_library(lithium::beryl ALIAS lithium_beryl)

target_include_directories(lithium_beryl INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lithium_beryl INTERFACE
    Lithium::core
)

# Platform-specific font backends
if(WIN32)
    # DirectWrite Backend
    add_subdirectory(src/backends/directwrite)
    target_link_libraries(lithium_beryl INTERFACE lithium_beryl_directwrite)
elseif(ANDROID OR LINUX)
    # FreeType Backend
    # add_subdirectory(src/backends/freetype)
    # target_link_libraries(lithium_beryl INTERFACE lithium_beryl_freetype)
elseif(APPLE)
    # CoreText Backend
    # add_subdirectory(src/backends/coretext)
    # target_link_libraries(lithium_beryl INTERFACE lithium_beryl_coretext)
endif()

# HarfBuzz for text shaping (if available)
if(HARFBUZZ_FOUND)
    target_link_libraries(lithium_beryl INTERFACE harfbuzz)
    target_compile_definitions(lithium_beryl INTERFACE BERYL_HAS_HARFBUZZ)
endif()
