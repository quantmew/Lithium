# Mica Graphics Engine
add_library(lithium_mica
    src/engine.cpp
    src/backend.cpp
    src/painter.cpp
)
add_library(lithium::mica ALIAS lithium_mica)

target_include_directories(lithium_mica PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lithium_mica PUBLIC
    Lithium::core
    lithium_beryl
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(lithium_mica PUBLIC
        d2d1
        dwrite
        windowscodecs
    )
endif()

# Direct2D Backend
if(WIN32)
    add_subdirectory(src/backends/direct2d)
    target_link_libraries(lithium_mica PUBLIC lithium_mica_direct2d)
endif()

# OpenGL Backend
# TODO: Enable after fixing build errors
# if(OPENGL_FOUND OR OPENGLES2_FOUND)
#     add_subdirectory(src/backends/opengl)
#     target_link_libraries(lithium_mica PUBLIC lithium_mica_opengl)
# endif()

# Software Backend (always available)
add_subdirectory(src/backends/software)
target_link_libraries(lithium_mica PUBLIC lithium_mica_software)
