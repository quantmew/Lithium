# Software Backend for Mica Graphics Engine

add_library(lithium_mica_software STATIC)
add_library(lithium::mica_software ALIAS lithium_mica_software)

target_sources(lithium_mica_software PRIVATE
    sw_backend.cpp
    sw_context.cpp
    sw_painter.cpp
    sw_resource.cpp
)

target_include_directories(lithium_mica_software PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

# Remove circular dependency - don't link mica here
target_link_libraries(lithium_mica_software PRIVATE
    Lithium::core
)

# Compile definitions
target_compile_definitions(lithium_mica_software PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CRT_SECURE_NO_WARNINGS
)

# Set C++ standard
target_compile_features(lithium_mica_software PRIVATE cxx_std_20)
