#pragma once

#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/graphics_config.hpp"
#include "lithium/core/types.hpp"
#include <memory>
#include <stack>

#ifdef _WIN32
    // Forward declarations for Direct2D/D3D types
    // These need to be forward declared properly to avoid conflicts
    #ifndef _D2D1_H_
    typedef struct ID2D1Factory ID2D1Factory;
    #endif
    #ifndef _D2D1_1_H_
    typedef struct ID2D1Device ID2D1Device;
    typedef struct ID2D1DeviceContext ID2D1DeviceContext;
    typedef struct ID2D1Bitmap ID2D1Bitmap;
    typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;
    #endif
    #ifndef _D3D11_H_
    typedef struct ID3D11Device ID3D11Device;
    typedef struct ID3D11DeviceContext ID3D11DeviceContext;
    #endif
    #ifndef _DXGI_H_
    typedef struct IDXGISwapChain IDXGISwapChain;
    #endif
    typedef unsigned long long UINT64;
#endif

namespace lithium::platform {

// Forward declarations
class D2DRenderState;
class D2DTextureCache;
class D2DTextRenderer;

// ============================================================================
// Direct2D Graphics Context
// ============================================================================

/**
 * @brief Direct2D 1.1 hardware-accelerated graphics context for Windows
 */
class D2DGraphicsContext : public GraphicsContext {
public:
    /**
     * @brief Create a Direct2D graphics context
     * @param window The window to render to
     * @param config Graphics configuration
     * @return Unique pointer to context or nullptr on failure
     */
    [[nodiscard]] static std::unique_ptr<D2DGraphicsContext>
    create(Window* window, const GraphicsConfig& config);

    ~D2DGraphicsContext() override;

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

    // Text measurement
    f32 measure_text(const String& text, f32 size) override;
    SizeF measure_text_size(const String& text, f32 size) override;

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

    // Direct2D-specific methods
    /**
     * @brief Get the native Direct2D device context
     */
    [[nodiscard]] ID2D1DeviceContext* native_context() const { return m_d2d_context; }

    /**
     * @brief Check if VSync is enabled
     */
    [[nodiscard]] bool vsync_enabled() const { return m_vsync_enabled; }

    /**
     * @brief Handle device loss (e.g., display adapter change)
     */
    [[nodiscard]] bool handle_device_loss();

private:
    D2DGraphicsContext(
        Window* window,
        const GraphicsConfig& config
    );

    // Initialization
    [[nodiscard]] bool initialize(const GraphicsConfig& config);
    void cleanup();

    // Device creation
    [[nodiscard]] bool create_d2d_factory();
    [[nodiscard]] bool create_d3d_device();
    [[nodiscard]] bool create_d2d_device();
    [[nodiscard]] bool create_swap_chain();
    [[nodiscard]] bool create_back_buffer();

    // Resize handling
    void handle_resize();

    // Window
    Window* m_window;

    // Direct2D objects
    ID2D1Factory* m_d2d_factory{nullptr};
    ID2D1Device* m_d2d_device{nullptr};
    ID2D1DeviceContext* m_d2d_context{nullptr};
    ID2D1Bitmap* m_back_buffer{nullptr};

    // Direct3D 11 objects
    ID3D11Device* m_d3d_device{nullptr};
    void* m_d3d_context{nullptr};  // ID3D11DeviceContext*

    // DXGI objects
    IDXGISwapChain* m_swap_chain{nullptr};

    // Configuration
    bool m_vsync_enabled{false};
    i32 m_msaa_samples{0};

    // State management
    std::unique_ptr<D2DRenderState> m_render_state;
    std::unique_ptr<D2DTextureCache> m_texture_cache;
    std::unique_ptr<D2DTextRenderer> m_text_renderer;

    // Transform stack (Direct2D Matrix3x2F)
    struct Transform {
        f32 x, y;
        f32 scale_x, scale_y;
        f32 rotation;
        f32 matrix[6];  // 3x2 transformation matrix
    };
    std::stack<Transform> m_transform_stack;
    Transform m_current_transform{};

    // Clip stack
    std::stack<RectF> m_clip_stack;

    // Opacity stack
    std::stack<f32> m_opacity_stack;
    f32 m_current_opacity{1.0f};

    // Viewport
    RectI m_viewport{};

    // Current state
    bool m_in_frame{false};
    UINT64 m_frame_counter{0};
};

} // namespace lithium::platform
