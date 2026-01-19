# Direct2D Backend for Mica Graphics Engine

add_library(lithium_mica_direct2d STATIC)
add_library(lithium::mica_direct2d ALIAS lithium_mica_direct2d)

target_sources(lithium_mica_direct2d PRIVATE
    d2d_backend.cpp
    d2d_painter.cpp
    d2d_context.cpp
    d2d_resource.cpp
)

target_include_directories(lithium_mica_direct2d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

# Remove circular dependency - don't link mica here
# But we need beryl for font types
target_link_libraries(lithium_mica_direct2d PRIVATE
    Lithium::core
    lithium_beryl
    d2d1
    dwrite
    d3d11
    dxgi
    windowscodecs
)

# Compile definitions
target_compile_definitions(lithium_mica_direct2d PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CRT_SECURE_NO_WARNINGS
)

# Set C++ standard
target_compile_features(lithium_mica_direct2d PRIVATE cxx_std_20)
