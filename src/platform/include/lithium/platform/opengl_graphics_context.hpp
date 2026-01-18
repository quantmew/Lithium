#pragma once

#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/graphics_config.hpp"
#include "lithium/core/types.hpp"
#include <memory>
#include <stack>
#include <array>

// Forward declarations for OpenGL types
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLsizei;
typedef unsigned int GLsizeiptr;

namespace lithium::platform {

// Forward declarations
class OpenGLShaderManager;
class OpenGLRenderState;
class OpenGLTextureCache;
struct OpenGLTexture;

// ============================================================================
// OpenGL Graphics Context
// ============================================================================

/**
 * @brief OpenGL 3.3+ Core Profile graphics context implementation
 */
class OpenGLGraphicsContext : public GraphicsContext {
public:
    /**
     * @brief Create an OpenGL graphics context
     * @param window The window to render to
     * @param config Graphics configuration
     * @return Unique pointer to context or nullptr on failure
     */
    [[nodiscard]] static std::unique_ptr<OpenGLGraphicsContext>
    create(Window* window, const GraphicsConfig& config);

    ~OpenGLGraphicsContext() override;

    // Context management
    void make_current() override;
    void swap_buffers() override;

    // Frame operations
    void begin_frame() override;
    void end_frame() override;

    // Clear
    void clear(const Color& color) override;

    // Basic drawing primitives
    void fill_rect(const RectF& rect, const Color& color) override;
    void stroke_rect(const RectF& rect, const Color& color, f32 width) override;

    void draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) override;

    // Text drawing
    void draw_text(const PointF& position, const String& text, const Color& color, f32 size) override;

    // Bitmap drawing
    void draw_bitmap(const RectF& dest, const Bitmap& bitmap) override;
    void draw_bitmap(const RectF& dest, const RectF& src, const Bitmap& bitmap) override;

    // Clipping
    void push_clip(const RectF& rect) override;
    void pop_clip() override;

    // Transform
    void push_transform() override;
    void pop_transform() override;
    void translate(f32 x, f32 y) override;
    void scale(f32 x, f32 y) override;
    void rotate(f32 radians) override;

    // Opacity
    void push_opacity(f32 opacity) override;
    void pop_opacity() override;

    // Viewport
    SizeI viewport_size() const override;
    void set_viewport(const RectI& rect) override;

    // OpenGL-specific methods
    /**
     * @brief Get the native OpenGL context handle
     */
    [[nodiscard]] void* native_context() const { return m_gl_context; }

    /**
     * @brief Check if VSync is enabled
     */
    [[nodiscard]] bool vsync_enabled() const { return m_vsync_enabled; }

    /**
     * @brief Set VSync state
     */
    void set_vsync(bool enabled);

    /**
     * @brief Get the actual OpenGL version
     */
    [[nodiscard]] std::pair<i32, i32> opengl_version() const {
        return {m_gl_major, m_gl_minor};
    }

private:
    OpenGLGraphicsContext(
        Window* window,
        void* gl_context,
        const GraphicsConfig& config
    );

    // Initialization
    [[nodiscard]] bool initialize(const GraphicsConfig& config);
    void cleanup();

    // OpenGL loading
    [[nodiscard]] bool load_opengl_functions();
    void setup_opengl_state();

    // Rendering helpers
    void flush_batch();
    void ensure_batch_space(usize vertex_count, usize index_count);

    // Window and context
    Window* m_window;
    void* m_gl_context;  // Platform-specific context (HGLRC on Windows, GLXContext on Linux)

    // OpenGL version
    i32 m_gl_major{0};
    i32 m_gl_minor{0};

    // Configuration
    bool m_vsync_enabled{false};
    i32 m_msaa_samples{0};

    // OpenGL objects
    GLuint m_vao{0};
    GLuint m_vbo{0};
    GLuint m_ebo{0};
    GLuint m_white_texture{0};  // 1x1 white texture for solid color rendering

    // Batch rendering
    static constexpr usize MAX_VERTICES = 4096 * 4;
    static constexpr usize MAX_INDICES = 4096 * 6;
    static constexpr usize MAX_TEXTURES = 16;

    struct Vertex {
        f32 x, y;           // Position
        f32 u, v;           // Texture coordinates
        u8 r, g, b, a;      // Color
        f32 texture_id;     // Texture slot index
    };

    std::vector<Vertex> m_vertices;
    std::vector<u32> m_indices;
    usize m_vertex_count{0};
    usize m_index_count{0};

    std::array<GLuint, MAX_TEXTURES> m_texture_slots{};
    usize m_texture_slot_count{0};

    // State management
    std::unique_ptr<OpenGLShaderManager> m_shader_manager;
    std::unique_ptr<OpenGLRenderState> m_render_state;
    std::unique_ptr<OpenGLTextureCache> m_texture_cache;

    // Transform stack
    struct Transform {
        f32 x, y;
        f32 scale_x, scale_y;
        f32 rotation;
        f32 matrix[16];  // 4x4 transformation matrix
    };
    std::stack<Transform> m_transform_stack;
    Transform m_current_transform{};

    // Clip stack
    std::stack<RectI> m_clip_stack;

    // Opacity stack
    std::stack<f32> m_opacity_stack;
    f32 m_current_opacity{1.0f};

    // Viewport
    RectI m_viewport{};

    // Current state
    bool m_in_frame{false};
    bool m_batch_dirty{false};
};

} // namespace lithium::platform
