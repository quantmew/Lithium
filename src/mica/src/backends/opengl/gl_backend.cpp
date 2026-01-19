/**
 * OpenGL Backend for Mica Graphics Engine
 */

#include "gl_backend.hpp"
#include <iostream>

namespace lithium::mica::opengl {

// ============================================================================
// GLBackend
// ============================================================================

GLBackend::GLBackend() {
    // Initialize capabilities (will be queried when GL context is created)
    m_capabilities.supports_multisampling = true;
    m_capabilities.supports_shaders = true;
    m_capabilities.supports_compute = false;  // TODO: Check for compute shader support
    m_capabilities.supports_framebuffer_srgb = true;
    m_capabilities.max_texture_size = 4096;  // Will be queried from GL
    m_capabilities.max_texture_units = 16;   // Will be queried
    m_capabilities.max_render_targets = 1;   // TODO: Check for FBO support
    m_capabilities.max_vertex_attributes = 16;
    m_capabilities.max_uniform_buffer_bindings = 0;  // TODO: Check for UBO support
    m_capabilities.max_anisotropy = 16.0f;

    // MSAA sample counts (common values)
    m_capabilities.sample_counts = {1, 2, 4, 8};
}

GLBackend::~GLBackend() = default;

bool GLBackend::initialize() {
    // Note: GL capabilities will be queried when a context is created
    std::cout << "GLBackend: Initialized" << std::endl;
    m_initialized = true;
    return true;
}

bool GLBackend::query_capabilities() {
    // TODO: Query actual GL capabilities
    // This requires an active GL context
    return true;
}

std::unique_ptr<Context> GLBackend::create_context(
    NativeWindowHandle window,
    const SwapChainConfig& config) {

#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(window.hwnd);
    if (!hwnd || !IsWindow(hwnd)) {
        std::cerr << "GLBackend: Invalid window handle" << std::endl;
        return nullptr;
    }
#elif defined(__linux__)
    if (!window.display || !window.window) {
        std::cerr << "GLBackend: Invalid X11 window" << std::endl;
        return nullptr;
    }
#endif

    auto context = std::make_unique<GLContext>(*this, window, config);
    if (!context || !context->is_valid()) {
        std::cerr << "GLBackend: Failed to create context" << std::endl;
        return nullptr;
    }

    return context;
}

std::unique_ptr<RenderTarget> GLBackend::create_render_target(
    const RenderTargetDesc& desc) {

    // TODO: Implement framebuffer object (FBO) render targets
    std::cerr << "GLBackend: Off-screen render targets not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Texture> GLBackend::create_texture(
    i32 width,
    i32 height,
    ImageFormat format,
    const void* data,
    i32 stride) {

    // TODO: Implement texture creation
    std::cerr << "GLBackend: Texture creation not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Buffer> GLBackend::create_buffer(
    BufferType type,
    BufferUsage usage,
    usize size,
    const void* data) {

    // TODO: Implement vertex/index/uniform buffer creation
    std::cerr << "GLBackend: Buffer creation not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Shader> GLBackend::create_shader(
    const String& vertex_source,
    const String& fragment_source) {

    // TODO: Implement shader program creation
    std::cerr << "GLBackend: Shader creation not yet implemented" << std::endl;
    return nullptr;
}

void GLBackend::flush() {
    glFlush();
}

void GLBackend::finish() {
    glFinish();
}

} // namespace lithium::mica::opengl
