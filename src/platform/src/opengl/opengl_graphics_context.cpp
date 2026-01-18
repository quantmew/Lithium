/**
 * OpenGL Graphics Context Implementation
 *
 * Implements a hardware-accelerated rendering backend using OpenGL 3.3+ Core Profile.
 * Features batch rendering, texture caching, and platform-specific context creation.
 */

#include "lithium/platform/opengl_graphics_context.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>

// Platform-specific OpenGL headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <wingdi.h>

    // OpenGL function pointers (will be loaded with GLAD)
    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXT)(HDC);
    typedef BOOL (WINAPI *PFNWGLDELETECONTEXT)(HGLRC);
    typedef BOOL (WINAPI *PFNWGLMAKECURRENT)(HDC, HGLRC);
    typedef BOOL (WINAPI *PFNWGLSWAPBUFFERS)(HDC);
    typedef PROC (WINAPI *PFNWGLGETPROCADDRESS)(LPCSTR);

    // OpenGL function pointers (will be loaded with GLAD)
    typedef HGLRC (WINAPI *PFNWGLCREATECONTEXT)(HDC);
    typedef BOOL (WINAPI *PFNWGLDELETECONTEXT)(HGLRC);
    typedef BOOL (WINAPI *PFNWGLMAKECURRENT)(HDC, HGLRC);
    typedef BOOL (WINAPI *PFNWGLSWAPBUFFERS)(HDC);
    typedef PROC (WINAPI *PFNWGLGETPROCADDRESS)(LPCSTR);

#elif defined(__linux__)
    #include <GL/glx.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

// GLAD OpenGL loader (generated, single-header configuration)
// For now, we'll use a minimal set of OpenGL function pointers
// In production, you would use the actual GLAD-generated loader

// Minimal OpenGL function declarations
extern "C" {
    // Core OpenGL 3.3+ functions
    typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    typedef void (APIENTRYP PFNGLCLEARPROC)(GLbitfield mask);
    typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
    typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
    typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
    typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
    typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
    typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
    typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices);
    typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
    typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);

    // Function pointers (loaded at runtime)
    static PFNGLCLEARCOLORPROC glClearColor_ptr = nullptr;
    static PFNGLCLEARPROC glClear_ptr = nullptr;
    static PFNGLVIEWPORTPROC glViewport_ptr = nullptr;
    static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ptr = nullptr;
    static PFNGLGENBUFFERSPROC glGenBuffers_ptr = nullptr;
    static PFNGLBINDBUFFERPROC glBindBuffer_ptr = nullptr;
    static PFNGLBUFFERDATAPROC glBufferData_ptr = nullptr;
    static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ptr = nullptr;
    static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr = nullptr;
    static PFNGLUSEPROGRAMPROC glUseProgram_ptr = nullptr;
    static PFNGLDRAWELEMENTSPROC glDrawElements_ptr = nullptr;
    static PFNGLDELETEBUFFERSPROC glDeleteBuffers_ptr = nullptr;
    static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ptr = nullptr;
}

// OpenGL constants
namespace GL {
    constexpr GLenum ARRAY_BUFFER = 0x8892;
    constexpr GLenum ELEMENT_ARRAY_BUFFER = 0x8893;
    constexpr GLenum STATIC_DRAW = 0x88E4;
    constexpr GLenum DYNAMIC_DRAW = 0x88E8;
    constexpr GLenum TRIANGLES = 0x0004;
    constexpr GLenum UNSIGNED_INT = 0x1405;
    constexpr GLenum FLOAT = 0x1406;
    constexpr GLenum COLOR_BUFFER_BIT = 0x00004000;
    constexpr GLenum DEPTH_BUFFER_BIT = 0x00000100;
    constexpr GLenum VERTEX_SHADER = 0x8B31;
    constexpr GLenum FRAGMENT_SHADER = 0x8B30;
    constexpr GLenum COMPILE_STATUS = 0x8B81;
    constexpr GLenum LINK_STATUS = 0x8B82;
    constexpr GLenum INFO_LOG_LENGTH = 0x8B84;
    constexpr GLenum TEXTURE_2D = 0x0DE1;
    constexpr GLenum RGBA = 0x1908;
    constexpr GLenum UNSIGNED_BYTE = 0x1401;
    constexpr GLenum TEXTURE0 = 0x84C0;
    constexpr GLenum TEXTURE1 = 0x84C1;
    constexpr GLenum TEXTURE_MIN_FILTER = 0x2801;
    constexpr GLenum TEXTURE_MAG_FILTER = 0x2800;
    constexpr GLenum LINEAR = 0x2601;
    constexpr GLenum CLAMP_TO_EDGE = 0x812F;
    constexpr GLenum TEXTURE_WRAP_S = 0x2802;
    constexpr GLenum TEXTURE_WRAP_T = 0x2803;
    constexpr GLenum SCISSOR_TEST = 0x0C11;
    constexpr GLenum BLEND = 0x0BE2;
    constexpr GLenum SRC_ALPHA = 0x0302;
    constexpr GLenum ONE_MINUS_SRC_ALPHA = 0x0303;
}

namespace lithium::platform {

// ============================================================================
// OpenGL Shader Manager (simplified internal implementation)
// ============================================================================

class OpenGLShaderManager {
public:
    OpenGLShaderManager() : m_program(0) {}
    ~OpenGLShaderManager() {
        // TODO: Delete shader program
    }

    bool initialize() {
        // TODO: Compile and link shaders
        // For now, return true
        return true;
    }

    void bind() {
        if (m_program != 0 && glUseProgram_ptr) {
            glUseProgram_ptr(m_program);
        }
    }

private:
    GLuint m_program;
};

// ============================================================================
// OpenGL Render State (simplified internal implementation)
// ============================================================================

class OpenGLRenderState {
public:
    OpenGLRenderState() = default;

    void set_scissor_enabled(bool enabled) {
        // TODO: Implement scissor test
        (void)enabled;
    }

    void set_scissor_rect(const RectI& rect) {
        // TODO: Implement scissor rect
        (void)rect;
    }

    void set_blend_enabled(bool enabled) {
        // TODO: Implement blend enable/disable
        (void)enabled;
    }

private:
};

// ============================================================================
// OpenGL Texture Cache (simplified internal implementation)
// ============================================================================

class OpenGLTextureCache {
public:
    OpenGLTextureCache() = default;
    ~OpenGLTextureCache() = default;

    GLuint upload_bitmap(const Bitmap& bitmap) {
        // TODO: Implement texture upload
        (void)bitmap;
        return 0;
    }

    void release_texture(GLuint texture_id) {
        // TODO: Implement texture release
        (void)texture_id;
    }

private:
};

// ============================================================================
// OpenGLGraphicsContext Implementation
// ============================================================================

std::unique_ptr<OpenGLGraphicsContext>
OpenGLGraphicsContext::create(Window* window, const GraphicsConfig& config) {
    if (!window) {
        LITHIUM_LOG_ERROR("Invalid window pointer");
        return nullptr;
    }

    // Platform-specific context creation
    void* gl_context = nullptr;

    #ifdef _WIN32
        // Windows WGL context creation
        HWND hwnd = static_cast<HWND>(window->native_handle());
        if (!hwnd) {
            LITHIUM_LOG_ERROR("Invalid window handle");
            return nullptr;
        }

        HDC hdc = GetDC(hwnd);
        if (!hdc) {
            LITHIUM_LOG_ERROR("Failed to get device context");
            return nullptr;
        }

        // Pixel format setup
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pixel_format = ChoosePixelFormat(hdc, &pfd);
        if (!pixel_format) {
            LITHIUM_LOG_ERROR("Failed to choose pixel format");
            ReleaseDC(hwnd, hdc);
            return nullptr;
        }

        if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
            LITHIUM_LOG_ERROR("Failed to set pixel format");
            ReleaseDC(hwnd, hdc);
            return nullptr;
        }

        // Load WGL functions
        HMODULE gl_module = LoadLibraryA("opengl32.dll");
        if (!gl_module) {
            LITHIUM_LOG_ERROR("Failed to load opengl32.dll");
            ReleaseDC(hwnd, hdc);
            return nullptr;
        }

        PFNWGLCREATECONTEXT wglCreateContext = (PFNWGLCREATECONTEXT)GetProcAddress(gl_module, "wglCreateContext");
        if (!wglCreateContext) {
            LITHIUM_LOG_ERROR("Failed to get wglCreateContext");
            FreeLibrary(gl_module);
            ReleaseDC(hwnd, hdc);
            return nullptr;
        }

        HGLRC hglrc = wglCreateContext(hdc);
        if (!hglrc) {
            LITHIUM_LOG_ERROR("Failed to create OpenGL context");
            FreeLibrary(gl_module);
            ReleaseDC(hwnd, hdc);
            return nullptr;
        }

        PFNWGLMAKECURRENT wglMakeCurrent = (PFNWGLMAKECURRENT)GetProcAddress(gl_module, "wglMakeCurrent");
        if (wglMakeCurrent) {
            wglMakeCurrent(hdc, hglrc);
        }

        gl_context = hglrc;

        // Store HDC for swap buffers
        window->set_property("hdc", hdc);

    #elif defined(__linux__)
        // Linux GLX context creation (stub)
        LITHIUM_LOG_ERROR("Linux GLX context creation not yet implemented");
        return nullptr;
    #endif

    // Create context object
    auto context = std::unique_ptr<OpenGLGraphicsContext>(
        new OpenGLGraphicsContext(window, gl_context, config)
    );

    if (!context->initialize(config)) {
        LITHIUM_LOG_ERROR("Failed to initialize OpenGL context");
        return nullptr;
    }

    return context;
}

OpenGLGraphicsContext::OpenGLGraphicsContext(
    Window* window,
    void* gl_context,
    const GraphicsConfig& config
)
    : m_window(window)
    , m_gl_context(gl_context)
    , m_vsync_enabled(config.enable_vsync)
    , m_msaa_samples(config.msaa_samples)
    , m_vertices()
    , m_indices()
{
    m_vertices.reserve(MAX_VERTICES);
    m_indices.reserve(MAX_INDICES);
    m_texture_slots.fill(0);
}

OpenGLGraphicsContext::~OpenGLGraphicsContext() {
    cleanup();
}

bool OpenGLGraphicsContext::initialize(const GraphicsConfig& config) {
    // Load OpenGL functions
    if (!load_opengl_functions()) {
        LITHIUM_LOG_ERROR("Failed to load OpenGL functions");
        return false;
    }

    // Query OpenGL version
    // TODO: Use glGetString(GL_VERSION) to get version

    // Setup initial OpenGL state
    setup_opengl_state();

    // Create shader manager
    m_shader_manager = std::make_unique<OpenGLShaderManager>();
    if (!m_shader_manager->initialize()) {
        LITHIUM_LOG_ERROR("Failed to initialize shader manager");
        return false;
    }

    // Create render state manager
    m_render_state = std::make_unique<OpenGLRenderState>();

    // Create texture cache
    m_texture_cache = std::make_unique<OpenGLTextureCache>();

    // Create white texture for solid color rendering
    u8 white_pixels[4] = {255, 255, 255, 255};
    Bitmap white_bitmap = {white_pixels, 1, 1, 4, Bitmap::Format::RGBA8};
    m_white_texture = m_texture_cache->upload_bitmap(white_bitmap);

    // Setup initial viewport
    auto size = m_window->framebuffer_size();
    m_viewport = {0, 0, size.width, size.height};

    // Initialize transform
    m_current_transform = {0, 0, 1, 1, 0, {}};
    std::memset(m_current_transform.matrix, 0, sizeof(m_current_transform.matrix));
    m_current_transform.matrix[0] = 1.0f;  // Identity matrix
    m_current_transform.matrix[5] = 1.0f;
    m_current_transform.matrix[10] = 1.0f;
    m_current_transform.matrix[15] = 1.0f;

    return true;
}

void OpenGLGraphicsContext::cleanup() {
    // Cleanup OpenGL objects
    if (glDeleteBuffers_ptr && m_vbo != 0) {
        glDeleteBuffers_ptr(1, &m_vbo);
        m_vbo = 0;
    }

    if (glDeleteBuffers_ptr && m_ebo != 0) {
        glDeleteBuffers_ptr(1, &m_ebo);
        m_ebo = 0;
    }

    if (glDeleteVertexArrays_ptr && m_vao != 0) {
        glDeleteVertexArrays_ptr(1, &m_vao);
        m_vao = 0;
    }

    // Platform-specific cleanup
    #ifdef _WIN32
        if (m_gl_context) {
            HGLRC hglrc = static_cast<HGLRC>(m_gl_context);
            HWND hwnd = static_cast<HWND>(m_window->native_handle());
            HDC hdc = GetDC(hwnd);

            PFNWGLMAKECURRENT wglMakeCurrent = (PFNWGLMAKECURRENT)GetProcAddress(
                GetModuleHandleA("opengl32.dll"), "wglMakeCurrent"
            );
            if (wglMakeCurrent) {
                wglMakeCurrent(nullptr, nullptr);
            }

            PFNWGLDELETECONTEXT wglDeleteContext = (PFNWGLDELETECONTEXT)GetProcAddress(
                GetModuleHandleA("opengl32.dll"), "wglDeleteContext"
            );
            if (wglDeleteContext) {
                wglDeleteContext(hglrc);
            }

            ReleaseDC(hwnd, hdc);
        }
    #endif
}

bool OpenGLGraphicsContext::load_opengl_functions() {
    // Load OpenGL 3.3+ core functions
    // In production, this would use GLAD-generated loaders

    #ifdef _WIN32
        HMODULE gl_module = GetModuleHandleA("opengl32.dll");
        if (!gl_module) {
            return false;
        }

        #define LOAD_GL_FUNCTION(type, name) \
            name##_ptr = (type)wglGetProcAddress(#name); \
            if (!name##_ptr) { \
                name##_ptr = (type)GetProcAddress(gl_module, #name); \
            }

        PFNWGLGETPROCADDRESS wglGetProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress(
            gl_module, "wglGetProcAddress"
        );

        if (!wglGetProcAddress) {
            return false;
        }

        LOAD_GL_FUNCTION(PFNGLCLEARCOLORPROC, glClearColor);
        LOAD_GL_FUNCTION(PFNGLCLEARPROC, glClear);
        LOAD_GL_FUNCTION(PFNGLVIEWPORTPROC, glViewport);
        LOAD_GL_FUNCTION(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
        LOAD_GL_FUNCTION(PFNGLGENBUFFERSPROC, glGenBuffers);
        LOAD_GL_FUNCTION(PFNGLBINDBUFFERPROC, glBindBuffer);
        LOAD_GL_FUNCTION(PFNGLBUFFERDATAPROC, glBufferData);
        LOAD_GL_FUNCTION(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
        LOAD_GL_FUNCTION(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
        LOAD_GL_FUNCTION(PFNGLUSEPROGRAMPROC, glUseProgram);
        LOAD_GL_FUNCTION(PFNGLDRAWELEMENTSPROC, glDrawElements);
        LOAD_GL_FUNCTION(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
        LOAD_GL_FUNCTION(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);

        // Check critical functions
        if (!glClear_ptr || !glViewport_ptr || !glGenBuffers_ptr) {
            LITHIUM_LOG_ERROR("Failed to load critical OpenGL functions");
            return false;
        }

    #else
        // Linux/Mac function loading would go here
        // For now, return false
        return false;
    #endif

    return true;
}

void OpenGLGraphicsContext::setup_opengl_state() {
    // Set clear color
    if (glClearColor_ptr) {
        glClearColor_ptr(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Enable blending for transparency
    // TODO: glEnable(GL_BLEND);
    // TODO: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ============================================================================
// Context Management
// ============================================================================

void OpenGLGraphicsContext::make_current() {
    #ifdef _WIN32
        HWND hwnd = static_cast<HWND>(m_window->native_handle());
        HDC hdc = GetDC(hwnd);

        PFNWGLMAKECURRENT wglMakeCurrent = (PFNWGLMAKECURRENT)GetProcAddress(
            GetModuleHandleA("opengl32.dll"), "wglMakeCurrent"
        );

        if (wglMakeCurrent) {
            wglMakeCurrent(hdc, static_cast<HGLRC>(m_gl_context));
        }

        ReleaseDC(hwnd, hdc);
    #endif
}

void OpenGLGraphicsContext::swap_buffers() {
    #ifdef _WIN32
        HWND hwnd = static_cast<HWND>(m_window->native_handle());
        HDC hdc = GetDC(hwnd);

        PFNWGLSWAPBUFFERS wglSwapBuffers = (PFNWGLSWAPBUFFERS)GetProcAddress(
            GetModuleHandleA("opengl32.dll"), "SwapBuffers"
        );

        if (wglSwapBuffers) {
            wglSwapBuffers(hdc);
        }

        ReleaseDC(hwnd, hdc);
    #endif
}

void OpenGLGraphicsContext::begin_frame() {
    m_in_frame = true;
    m_vertex_count = 0;
    m_index_count = 0;
    m_texture_slot_count = 0;

    auto size = m_window->framebuffer_size();
    if (size.width != m_viewport.width || size.height != m_viewport.height) {
        m_viewport = {0, 0, size.width, size.height};
        if (glViewport_ptr) {
            glViewport_ptr(0, 0, size.width, size.height);
        }
    }
}

void OpenGLGraphicsContext::end_frame() {
    flush_batch();
    m_in_frame = false;
}

// ============================================================================
// Drawing Operations
// ============================================================================

void OpenGLGraphicsContext::clear(const Color& color) {
    flush_batch();

    if (glClearColor_ptr) {
        glClearColor_ptr(
            color.r / 255.0f,
            color.g / 255.0f,
            color.b / 255.0f,
            color.a / 255.0f
        );
    }

    if (glClear_ptr) {
        glClear_ptr(GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT);
    }
}

void OpenGLGraphicsContext::fill_rect(const RectF& rect, const Color& color) {
    // TODO: Implement batch rendering
    (void)rect;
    (void)color;
}

void OpenGLGraphicsContext::stroke_rect(const RectF& rect, const Color& color, f32 width) {
    // TODO: Implement stroke rendering
    (void)rect;
    (void)color;
    (void)width;
}

void OpenGLGraphicsContext::draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) {
    // TODO: Implement line rendering
    (void)from;
    (void)to;
    (void)color;
    (void)width;
}

void OpenGLGraphicsContext::draw_text(const PointF& position, const String& text, const Color& color, f32 size) {
    // TODO: Implement text rendering (will use shared glyph cache)
    (void)position;
    (void)text;
    (void)color;
    (void)size;
}

f32 OpenGLGraphicsContext::measure_text(const String& text, f32 size) {
    // TODO: Implement text measurement (will use shared glyph cache)
    (void)text;
    (void)size;
    // Approximate: 6 pixels per character
    return static_cast<f32>(text.length() * 6);
}

SizeF OpenGLGraphicsContext::measure_text_size(const String& text, f32 size) {
    f32 width = measure_text(text, size);
    f32 height = size;  // Approximate height as font size
    return {width, height};
}

void OpenGLGraphicsContext::draw_bitmap(const RectF& dest, const Bitmap& bitmap) {
    draw_bitmap(dest, {0, 0, static_cast<f32>(bitmap.width), static_cast<f32>(bitmap.height)}, bitmap);
}

void OpenGLGraphicsContext::draw_bitmap(const RectF& dest, const RectF& src, const Bitmap& bitmap) {
    // TODO: Implement bitmap rendering with texture upload
    (void)dest;
    (void)src;
    (void)bitmap;
}

// ============================================================================
// State Management
// ============================================================================

void OpenGLGraphicsContext::push_clip(const RectF& rect) {
    RectI int_rect = {
        static_cast<i32>(rect.x),
        static_cast<i32>(rect.y),
        static_cast<i32>(rect.width),
        static_cast<i32>(rect.height)
    };

    if (!m_clip_stack.empty()) {
        // TODO: Intersect with current clip
    }

    m_clip_stack.push(int_rect);
    m_render_state->set_scissor_enabled(true);
    m_render_state->set_scissor_rect(int_rect);
}

void OpenGLGraphicsContext::pop_clip() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop();
    }

    if (m_clip_stack.empty()) {
        m_render_state->set_scissor_enabled(false);
    } else {
        m_render_state->set_scissor_rect(m_clip_stack.top());
    }
}

void OpenGLGraphicsContext::push_transform() {
    m_transform_stack.push(m_current_transform);
}

void OpenGLGraphicsContext::pop_transform() {
    if (!m_transform_stack.empty()) {
        m_current_transform = m_transform_stack.top();
        m_transform_stack.pop();
    }
}

void OpenGLGraphicsContext::translate(f32 x, f32 y) {
    m_current_transform.x += x;
    m_current_transform.y += y;
}

void OpenGLGraphicsContext::scale(f32 x, f32 y) {
    m_current_transform.scale_x *= x;
    m_current_transform.scale_y *= y;
}

void OpenGLGraphicsContext::rotate(f32 radians) {
    m_current_transform.rotation += radians;
}

void OpenGLGraphicsContext::push_opacity(f32 opacity) {
    m_opacity_stack.push(m_current_opacity);
    m_current_opacity *= opacity;
}

void OpenGLGraphicsContext::pop_opacity() {
    if (!m_opacity_stack.empty()) {
        m_current_opacity = m_opacity_stack.top();
        m_opacity_stack.pop();
    }
}

// ============================================================================
// Viewport
// ============================================================================

SizeI OpenGLGraphicsContext::viewport_size() const {
    return {m_viewport.width, m_viewport.height};
}

void OpenGLGraphicsContext::set_viewport(const RectI& rect) {
    m_viewport = rect;
    if (glViewport_ptr) {
        glViewport_ptr(rect.x, rect.y, rect.width, rect.height);
    }
}

void OpenGLGraphicsContext::set_vsync(bool enabled) {
    m_vsync_enabled = enabled;
    // TODO: Implement WGL/GLX swap interval control
}

// ============================================================================
// Batch Rendering
// ============================================================================

void OpenGLGraphicsContext::flush_batch() {
    if (m_vertex_count == 0 || m_index_count == 0) {
        return;
    }

    // TODO: Upload vertices to VBO
    // TODO: Upload indices to EBO
    // TODO: Draw elements

    m_vertex_count = 0;
    m_index_count = 0;
    m_texture_slot_count = 0;
}

void OpenGLGraphicsContext::ensure_batch_space(usize vertex_count, usize index_count) {
    if (m_vertex_count + vertex_count > MAX_VERTICES ||
        m_index_count + index_count > MAX_INDICES) {
        flush_batch();
    }
}

} // namespace lithium::platform
