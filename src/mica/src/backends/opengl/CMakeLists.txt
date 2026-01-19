# OpenGL Backend for Mica Graphics Engine

add_library(lithium_mica_opengl STATIC)
add_library(lithium::mica_opengl ALIAS lithium_mica_opengl)

target_sources(lithium_mica_opengl PRIVATE
    gl_backend.cpp
    gl_context.cpp
    gl_painter.cpp
    gl_resource.cpp
)

target_include_directories(lithium_mica_opengl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Find OpenGL
find_package(OpenGL REQUIRED)

target_link_libraries(lithium_mica_opengl PRIVATE
    Lithium::core
    OpenGL::GL
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(lithium_mica_opengl PRIVATE
        gdi32
        opengl32
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(lithium_mica_opengl PRIVATE
        GL
        X11
    )
endif()

# Compile definitions
target_compile_definitions(lithium_mica_opengl PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    CRT_SECURE_NO_WARNINGS
)

# Set C++ standard
target_compile_features(lithium_mica_opengl PRIVATE cxx_std_20)
