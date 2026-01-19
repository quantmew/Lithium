#pragma once

#include "lithium/mica/backend.hpp"
#include "lithium/mica/context.hpp"
#include "lithium/mica/painter.hpp"
#include "lithium/mica/resource.hpp"
#include <memory>
#include <vector>

// Forward declarations for beryl types
namespace lithium::beryl {
class FontDescription;
class TextLayout;
}

namespace lithium::mica::software {

// ============================================================================
// Software Backend
// ============================================================================

class SoftwareBackend : public IBackend {
public:
    SoftwareBackend();
    ~SoftwareBackend() override;

    [[nodiscard]] BackendType type() const noexcept override {
        return BackendType::Software;
    }

    [[nodiscard]] const BackendCapabilities& capabilities() const noexcept override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<Context> create_context(
        NativeWindowHandle window,
        const SwapChainConfig& config) override;

    [[nodiscard]] std::unique_ptr<RenderTarget> create_render_target(
        const RenderTargetDesc& desc) override;

    [[nodiscard]] std::unique_ptr<Texture> create_texture(
        i32 width,
        i32 height,
        ImageFormat format,
        const void* data,
        i32 stride) override;

    [[nodiscard]] std::unique_ptr<Buffer> create_buffer(
        BufferType type,
        BufferUsage usage,
        usize size,
        const void* data) override;

    [[nodiscard]] std::unique_ptr<Shader> create_shader(
        const String& vertex_source,
        const String& fragment_source) override;

    void flush() override;
    void finish() override;

    [[nodiscard]] bool initialize();

private:
    BackendCapabilities m_capabilities;
};

// ============================================================================
// Software Context
// ============================================================================>

class SoftwareContext : public Context {
public:
    SoftwareContext(SoftwareBackend& backend, NativeWindowHandle window, const SwapChainConfig& config);
    ~SoftwareContext() override;

    [[nodiscard]] IBackend* backend() noexcept override {
        return &m_backend;
    }

    [[nodiscard]] NativeWindowHandle native_window() const noexcept override {
        return m_window_handle;
    }

    [[nodiscard]] SwapChain* swap_chain() noexcept override;
    [[nodiscard]] std::unique_ptr<Painter> create_painter() override;
    [[nodiscard]] RenderTarget* current_render_target() noexcept override;
    void set_render_target(RenderTarget* target) override;

    bool resize(i32 width, i32 height) override;
    [[nodiscard]] Size size() const noexcept override;

    void begin_frame() override;
    void end_frame() override;
    void present() override;
    void flush() override;

    [[nodiscard]] f32 dpi_scale() const noexcept override;
    [[nodiscard]] bool is_valid() const noexcept override;

    // Frame buffer access
    [[nodiscard]] u32* frame_buffer() noexcept { return m_frame_buffer.data(); }
    [[nodiscard]] const u32* frame_buffer() const noexcept { return m_frame_buffer.data(); }

private:
    SoftwareBackend& m_backend;
    NativeWindowHandle m_window_handle;
    SwapChainConfig m_config;
    f32 m_dpi_scale{1.0f};

    std::vector<u32> m_frame_buffer;  // RGBA8 pixel data
    i32 m_width{0};
    i32 m_height{0};

    [[nodiscard]] bool allocate_frame_buffer();
};

// ============================================================================
// Software Painter
// ============================================================================>

class SoftwarePainter : public Painter {
public:
    SoftwarePainter(SoftwareContext& context);
    ~SoftwarePainter() override;

    [[nodiscard]] Context* context() noexcept override;
    void save() override;
    void restore() override;
    [[nodiscard]] const PainterState& state() const noexcept override;

    void translate(Vec2 offset) override;
    void scale(Vec2 factors) override;
    void rotate(f32 angle) override;
    void concat(const Mat3& matrix) override;
    void set_transform(const Mat3& transform) override;
    [[nodiscard]] const Mat3& transform() const noexcept override;

    void draw_line(Vec2 start, Vec2 end, const Paint& paint) override;
    void draw_rect(const Rect& rect, const Paint& paint) override;
    void fill_rect(const Rect& rect, const Paint& paint) override;
    void draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) override;
    void fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) override;
    void draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) override;
    void fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) override;
    void draw_circle(Vec2 center, f32 radius, const Paint& paint) override;
    void fill_circle(Vec2 center, f32 radius, const Paint& paint) override;
    void draw_path(const Path& path, const Paint& paint) override;
    void fill_path(const Path& path, const Paint& paint) override;

    void draw_text(Vec2 position, const String& text, const Paint& paint,
                  const beryl::FontDescription& font_desc) override;
    void draw_text_layout(Vec2 position, const beryl::TextLayout& layout,
                         const Paint& paint) override;

    void draw_image(Vec2 position, Texture* texture, const Paint& paint) override;
    void draw_image_rect(const Rect& dest, Texture* texture, const Rect& src,
                        const Paint& paint) override;
    void draw_image_tinted(const Rect& dest, Texture* texture, const Rect& src,
                          const Color& tint) override;

    void clip_rect(const Rect& rect) override;
    void clip_path(const Path& path) override;
    void reset_clip() override;

    void clear(const Color& color) override;

private:
    SoftwareContext& m_context;
    std::vector<std::unique_ptr<PainterState>> m_state_stack;
    PainterState m_current_state;

    // Helper functions
    void set_pixel(i32 x, i32 y, const Color& color);
    Color blend_pixel(i32 x, i32 y, const Color& color);
    void draw_h_line(i32 x1, i32 x2, i32 y, const Color& color);
    void draw_v_line(i32 x, i32 y1, i32 y2, const Color& color);
};

// Register the software backend factory
void register_software_backend_factory();

} // namespace lithium::mica::software
