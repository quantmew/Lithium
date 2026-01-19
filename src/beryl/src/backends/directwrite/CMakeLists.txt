# DirectWrite Font Backend for Beryl Text Engine

add_library(lithium_beryl_directwrite STATIC)
add_library(lithium::beryl_directwrite ALIAS lithium_beryl_directwrite)

target_sources(lithium_beryl_directwrite PRIVATE
    dwrite_font.cpp
    ../../backend.cpp
)

target_include_directories(lithium_beryl_directwrite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(lithium_beryl_directwrite PRIVATE
    Lithium::core
    dwrite
)

# Compile definitions
target_compile_definitions(lithium_beryl_directwrite PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CRT_SECURE_NO_WARNINGS
)

# Set C++ standard
target_compile_features(lithium_beryl_directwrite PRIVATE cxx_std_20)
