#pragma once

#include "lithium/core/types.hpp"
#include "window.hpp"
#include <memory>

namespace lithium::platform {

// Forward declarations
struct GraphicsConfig;

// ============================================================================
// Graphics Context - Platform graphics abstraction
// ============================================================================

class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;

    // Factory
    [[nodiscard]] static std::unique_ptr<GraphicsContext> create(Window* window);

    // Factory with backend configuration
    [[nodiscard]] static std::unique_ptr<GraphicsContext> create(
        Window* window,
        const GraphicsConfig& config
    );

    // Context management
    virtual void make_current() = 0;
    virtual void swap_buffers() = 0;

    // Frame operations
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;

    // Clear
    virtual void clear(const Color& color) = 0;

    // Basic drawing primitives
    virtual void fill_rect(const RectF& rect, const Color& color) = 0;
    virtual void stroke_rect(const RectF& rect, const Color& color, f32 width) = 0;

    virtual void draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) = 0;

    // Text drawing (basic - full text rendering is in text module)
    virtual void draw_text(const PointF& position, const String& text, const Color& color, f32 size) = 0;

    // Bitmap drawing
    struct Bitmap {
        u8* data;
        i32 width;
        i32 height;
        i32 stride;
        enum class Format { RGBA8, RGB8, A8 } format;
    };

    virtual void draw_bitmap(const RectF& dest, const Bitmap& bitmap) = 0;
    virtual void draw_bitmap(const RectF& dest, const RectF& src, const Bitmap& bitmap) = 0;

    // Clipping
    virtual void push_clip(const RectF& rect) = 0;
    virtual void pop_clip() = 0;

    // Transform
    virtual void push_transform() = 0;
    virtual void pop_transform() = 0;
    virtual void translate(f32 x, f32 y) = 0;
    virtual void scale(f32 x, f32 y) = 0;
    virtual void rotate(f32 radians) = 0;

    // Opacity
    virtual void push_opacity(f32 opacity) = 0;
    virtual void pop_opacity() = 0;

    // Viewport
    [[nodiscard]] virtual SizeI viewport_size() const = 0;
    virtual void set_viewport(const RectI& rect) = 0;

protected:
    GraphicsContext() = default;
};

// ============================================================================
// Bitmap utilities
// ============================================================================

class BitmapImage {
public:
    BitmapImage() = default;
    BitmapImage(i32 width, i32 height, GraphicsContext::Bitmap::Format format);

    [[nodiscard]] i32 width() const { return m_width; }
    [[nodiscard]] i32 height() const { return m_height; }
    [[nodiscard]] i32 stride() const { return m_stride; }
    [[nodiscard]] GraphicsContext::Bitmap::Format format() const { return m_format; }

    [[nodiscard]] u8* data() { return m_data.data(); }
    [[nodiscard]] const u8* data() const { return m_data.data(); }

    [[nodiscard]] GraphicsContext::Bitmap as_bitmap() const {
        return {
            const_cast<u8*>(m_data.data()),
            m_width, m_height, m_stride, m_format
        };
    }

    // Pixel access
    void set_pixel(i32 x, i32 y, const Color& color);
    [[nodiscard]] Color get_pixel(i32 x, i32 y) const;

    // Fill
    void fill(const Color& color);
    void fill_rect(const RectI& rect, const Color& color);

    // Blit
    void blit(const BitmapImage& src, i32 dest_x, i32 dest_y);

private:
    i32 m_width{0};
    i32 m_height{0};
    i32 m_stride{0};
    GraphicsContext::Bitmap::Format m_format{GraphicsContext::Bitmap::Format::RGBA8};
    std::vector<u8> m_data;
};

} // namespace lithium::platform
