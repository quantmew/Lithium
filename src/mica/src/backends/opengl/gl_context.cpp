/**
 * OpenGL Context Implementation
 */

#include "gl_backend.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#include <X11/Xlib.h>
#endif

namespace lithium::mica::opengl {

// ============================================================================
// GLContext
// ============================================================================

GLContext::GLContext(GLBackend& backend, NativeWindowHandle window, const SwapChainConfig& config)
    : m_backend(backend)
    , m_window_handle(window)
    , m_config(config)
{
#ifdef _WIN32
    m_hwnd = static_cast<HWND>(window.hwnd);
    m_hdc = GetDC(m_hwnd);
#elif defined(__linux__)
    // X11 setup
#endif

    if (!create_gl_context()) {
        std::cerr << "GLContext: Failed to create GL context" << std::endl;
        return;
    }

    // Get DPI scale
#ifdef _WIN32
    HDC hdc = GetDC(m_hwnd);
    f32 dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
    m_dpi_scale = dpi_x / 96.0f;
    ReleaseDC(m_hwnd, hdc);
#endif

    std::cout << "GLContext: Initialized successfully (DPI scale: " << m_dpi_scale << ")" << std::endl;
}

GLContext::~GLContext() {
    destroy_gl_context();
}

SwapChain* GLContext::swap_chain() noexcept {
    // For OpenGL, swap chain is managed by the context
    return reinterpret_cast<SwapChain*>(this);
}

std::unique_ptr<Painter> GLContext::create_painter() {
    return std::make_unique<GLPainter>(*this);
}

RenderTarget* GLContext::current_render_target() noexcept {
    // Default framebuffer (0)
    return reinterpret_cast<RenderTarget*>(this);
}

void GLContext::set_render_target(RenderTarget* target) {
    // TODO: Implement render target switching with FBOs
    std::cerr << "GLContext: Render target switching not yet implemented" << std::endl;
}

bool GLContext::resize(i32 width, i32 height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    // Update viewport
    glViewport(0, 0, width, height);
    return true;
}

Size GLContext::size() const noexcept {
    // TODO: Query actual window size
    return {m_config.width, m_config.height};
}

void GLContext::begin_frame() {
    // Clear color and depth buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLContext::end_frame() {
    // Nothing to do for OpenGL
}

void GLContext::present() {
#ifdef _WIN32
    if (m_hdc) {
        SwapBuffers(m_hdc);
    }
#elif defined(__linux__)
    if (display()) {
        glXSwapBuffers(display(), m_window_handle.window);
    }
#endif
}

void GLContext::flush() {
    glFlush();
}

f32 GLContext::dpi_scale() const noexcept {
    return m_dpi_scale;
}

bool GLContext::is_valid() const noexcept {
#ifdef _WIN32
    return m_gl_context != nullptr && m_hdc != nullptr;
#elif defined(__linux__)
    return m_gl_context != nullptr;
#else
    return true;  // TODO: Implement for macOS
#endif
}

bool GLContext::create_gl_context() {
#ifdef _WIN32
    // Windows implementation
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixel_format = ChoosePixelFormat(m_hdc, &pfd);
    if (pixel_format == 0) {
        std::cerr << "GLContext: Failed to choose pixel format" << std::endl;
        return false;
    }

    if (!SetPixelFormat(m_hdc, pixel_format, &pfd)) {
        std::cerr << "GLContext: Failed to set pixel format" << std::endl;
        return false;
    }

    m_gl_context = wglCreateContext(m_hdc);
    if (!m_gl_context) {
        std::cerr << "GLContext: Failed to create GL context" << std::endl;
        return false;
    }

    if (!wglMakeCurrent(m_hdc, m_gl_context)) {
        std::cerr << "GLContext: Failed to make GL context current" << std::endl;
        return false;
    }

    // Setup initial viewport
    glViewport(0, 0, m_config.width, m_config.height);

    return true;

#elif defined(__linux__)
    // Linux/X11 implementation
    Display* display = reinterpret_cast<Display*>(m_window_handle.display);
    if (!display) {
        std::cerr << "GLContext: Invalid X11 display" << std::endl;
        return false;
    }

    GLint visual_attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_SAMPLE_BUFFERS, m_config.sample_count > 1 ? 1 : 0,
        GLX_SAMPLES, m_config.sample_count,
        None
    };

    m_visual_info = glXChooseVisual(display, DefaultScreen(display), visual_attribs);
    if (!m_visual_info) {
        std::cerr << "GLContext: Failed to choose visual" << std::endl;
        return false;
    }

    m_gl_context = glXCreateContext(display, m_visual_info, nullptr, GL_TRUE);
    if (!m_gl_context) {
        std::cerr << "GLContext: Failed to create GLX context" << std::endl;
        return false;
    }

    if (!glXMakeCurrent(display, m_window_handle.window, m_gl_context)) {
        std::cerr << "GLContext: Failed to make GLX context current" << std::endl;
        return false;
    }

    // Setup initial viewport
    glViewport(0, 0, m_config.width, m_config.height);

    return true;

#else
    std::cerr << "GLContext: Platform not yet implemented" << std::endl;
    return false;
#endif
}

void GLContext::destroy_gl_context() {
#ifdef _WIN32
    if (m_gl_context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_gl_context);
        m_gl_context = nullptr;
    }
    if (m_hdc) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }
#elif defined(__linux__)
    if (m_gl_context && display()) {
        glXDestroyContext(display(), m_gl_context);
        m_gl_context = nullptr;
    }
    if (m_visual_info) {
        XFree(m_visual_info);
        m_visual_info = nullptr;
    }
#endif
}

} // namespace lithium::mica::opengl
