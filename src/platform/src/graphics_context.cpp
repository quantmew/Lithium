/**
 * Graphics Context implementation (software rasterizer stub)
 */

#include "lithium/platform/graphics_context.hpp"
#include <algorithm>
#include <stack>
#include <cstring>

namespace lithium::platform {

// ============================================================================
// Software Graphics Context
// ============================================================================

class SoftwareGraphicsContext : public GraphicsContext {
public:
    explicit SoftwareGraphicsContext(Window* window)
        : m_window(window)
    {
        auto size = window->framebuffer_size();
        m_framebuffer = BitmapImage(size.width, size.height, Bitmap::Format::RGBA8);
        m_viewport = {0, 0, size.width, size.height};
    }

    void make_current() override {}
    void swap_buffers() override {}

    void begin_frame() override {
        auto size = m_window->framebuffer_size();
        if (size.width != m_framebuffer.width() || size.height != m_framebuffer.height()) {
            m_framebuffer = BitmapImage(size.width, size.height, Bitmap::Format::RGBA8);
            m_viewport = {0, 0, size.width, size.height};
        }
    }

    void end_frame() override {}

    void clear(const Color& color) override {
        m_framebuffer.fill(color);
    }

    void fill_rect(const RectF& rect, const Color& color) override {
        RectI int_rect = {
            static_cast<i32>(rect.x + m_transform_x),
            static_cast<i32>(rect.y + m_transform_y),
            static_cast<i32>(rect.width * m_scale_x),
            static_cast<i32>(rect.height * m_scale_y)
        };

        Color final_color = color;
        final_color.a = static_cast<u8>(final_color.a * m_opacity);

        if (!m_clip_stack.empty()) {
            int_rect = intersect(int_rect, m_clip_stack.top());
        }

        m_framebuffer.fill_rect(int_rect, final_color);
    }

    void stroke_rect(const RectF& rect, const Color& color, f32 width) override {
        // Draw four lines for the border
        draw_line({rect.x, rect.y}, {rect.x + rect.width, rect.y}, color, width);
        draw_line({rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + rect.height}, color, width);
        draw_line({rect.x + rect.width, rect.y + rect.height}, {rect.x, rect.y + rect.height}, color, width);
        draw_line({rect.x, rect.y + rect.height}, {rect.x, rect.y}, color, width);
    }

    void draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) override {
        // Simple Bresenham line drawing
        i32 x0 = static_cast<i32>(from.x + m_transform_x);
        i32 y0 = static_cast<i32>(from.y + m_transform_y);
        i32 x1 = static_cast<i32>(to.x + m_transform_x);
        i32 y1 = static_cast<i32>(to.y + m_transform_y);

        i32 dx = std::abs(x1 - x0);
        i32 dy = std::abs(y1 - y0);
        i32 sx = x0 < x1 ? 1 : -1;
        i32 sy = y0 < y1 ? 1 : -1;
        i32 err = dx - dy;

        Color final_color = color;
        final_color.a = static_cast<u8>(final_color.a * m_opacity);

        while (true) {
            if (is_in_clip(x0, y0)) {
                m_framebuffer.set_pixel(x0, y0, final_color);
            }

            if (x0 == x1 && y0 == y1) break;

            i32 e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void draw_text(const PointF& position, const String& text, const Color& color, f32 size) override {
        // Text rendering is a stub - would need font rasterization
    }

    void draw_bitmap(const RectF& dest, const Bitmap& bitmap) override {
        draw_bitmap(dest, {0, 0, static_cast<f32>(bitmap.width), static_cast<f32>(bitmap.height)}, bitmap);
    }

    void draw_bitmap(const RectF& dest, const RectF& src, const Bitmap& bitmap) override {
        // Simple nearest-neighbor scaling
        i32 dest_x = static_cast<i32>(dest.x + m_transform_x);
        i32 dest_y = static_cast<i32>(dest.y + m_transform_y);
        i32 dest_w = static_cast<i32>(dest.width * m_scale_x);
        i32 dest_h = static_cast<i32>(dest.height * m_scale_y);

        for (i32 y = 0; y < dest_h; ++y) {
            for (i32 x = 0; x < dest_w; ++x) {
                i32 px = dest_x + x;
                i32 py = dest_y + y;

                if (!is_in_clip(px, py)) continue;

                f32 u = (x / static_cast<f32>(dest_w)) * src.width + src.x;
                f32 v = (y / static_cast<f32>(dest_h)) * src.height + src.y;

                i32 src_x = static_cast<i32>(u);
                i32 src_y = static_cast<i32>(v);

                if (src_x < 0 || src_x >= bitmap.width ||
                    src_y < 0 || src_y >= bitmap.height) continue;

                i32 src_offset = src_y * bitmap.stride + src_x * 4;
                Color c = {
                    bitmap.data[src_offset],
                    bitmap.data[src_offset + 1],
                    bitmap.data[src_offset + 2],
                    static_cast<u8>(bitmap.data[src_offset + 3] * m_opacity)
                };
                m_framebuffer.set_pixel(px, py, c);
            }
        }
    }

    void push_clip(const RectF& rect) override {
        RectI int_rect = {
            static_cast<i32>(rect.x + m_transform_x),
            static_cast<i32>(rect.y + m_transform_y),
            static_cast<i32>(rect.width * m_scale_x),
            static_cast<i32>(rect.height * m_scale_y)
        };

        if (!m_clip_stack.empty()) {
            int_rect = intersect(int_rect, m_clip_stack.top());
        }
        m_clip_stack.push(int_rect);
    }

    void pop_clip() override {
        if (!m_clip_stack.empty()) {
            m_clip_stack.pop();
        }
    }

    void push_transform() override {
        m_transform_stack.push({m_transform_x, m_transform_y, m_scale_x, m_scale_y, m_rotation});
    }

    void pop_transform() override {
        if (!m_transform_stack.empty()) {
            auto& t = m_transform_stack.top();
            m_transform_x = t.x;
            m_transform_y = t.y;
            m_scale_x = t.scale_x;
            m_scale_y = t.scale_y;
            m_rotation = t.rotation;
            m_transform_stack.pop();
        }
    }

    void translate(f32 x, f32 y) override {
        m_transform_x += x;
        m_transform_y += y;
    }

    void scale(f32 x, f32 y) override {
        m_scale_x *= x;
        m_scale_y *= y;
    }

    void rotate(f32 radians) override {
        m_rotation += radians;
    }

    void push_opacity(f32 opacity) override {
        m_opacity_stack.push(m_opacity);
        m_opacity *= opacity;
    }

    void pop_opacity() override {
        if (!m_opacity_stack.empty()) {
            m_opacity = m_opacity_stack.top();
            m_opacity_stack.pop();
        }
    }

    SizeI viewport_size() const override {
        return {m_viewport.width, m_viewport.height};
    }

    void set_viewport(const RectI& rect) override {
        m_viewport = rect;
    }

private:
    bool is_in_clip(i32 x, i32 y) const {
        if (m_clip_stack.empty()) {
            return x >= 0 && y >= 0 && x < m_framebuffer.width() && y < m_framebuffer.height();
        }
        auto& clip = m_clip_stack.top();
        return x >= clip.x && y >= clip.y &&
               x < clip.x + clip.width && y < clip.y + clip.height;
    }

    RectI intersect(const RectI& a, const RectI& b) const {
        i32 x = std::max(a.x, b.x);
        i32 y = std::max(a.y, b.y);
        i32 right = std::min(a.x + a.width, b.x + b.width);
        i32 bottom = std::min(a.y + a.height, b.y + b.height);
        return {x, y, std::max(0, right - x), std::max(0, bottom - y)};
    }

    Window* m_window;
    BitmapImage m_framebuffer;
    RectI m_viewport;

    std::stack<RectI> m_clip_stack;

    struct Transform {
        f32 x, y;
        f32 scale_x, scale_y;
        f32 rotation;
    };
    std::stack<Transform> m_transform_stack;
    f32 m_transform_x{0};
    f32 m_transform_y{0};
    f32 m_scale_x{1};
    f32 m_scale_y{1};
    f32 m_rotation{0};

    std::stack<f32> m_opacity_stack;
    f32 m_opacity{1.0f};
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<GraphicsContext> GraphicsContext::create(Window* window) {
    return std::make_unique<SoftwareGraphicsContext>(window);
}

// ============================================================================
// BitmapImage implementation
// ============================================================================

BitmapImage::BitmapImage(i32 width, i32 height, GraphicsContext::Bitmap::Format format)
    : m_width(width)
    , m_height(height)
    , m_format(format)
{
    i32 bytes_per_pixel = 4;
    if (format == GraphicsContext::Bitmap::Format::RGB8) bytes_per_pixel = 3;
    else if (format == GraphicsContext::Bitmap::Format::A8) bytes_per_pixel = 1;

    m_stride = width * bytes_per_pixel;
    m_data.resize(m_stride * height);
}

void BitmapImage::set_pixel(i32 x, i32 y, const Color& color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;

    i32 offset = y * m_stride;
    switch (m_format) {
        case GraphicsContext::Bitmap::Format::RGBA8:
            offset += x * 4;
            m_data[offset] = color.r;
            m_data[offset + 1] = color.g;
            m_data[offset + 2] = color.b;
            m_data[offset + 3] = color.a;
            break;
        case GraphicsContext::Bitmap::Format::RGB8:
            offset += x * 3;
            m_data[offset] = color.r;
            m_data[offset + 1] = color.g;
            m_data[offset + 2] = color.b;
            break;
        case GraphicsContext::Bitmap::Format::A8:
            offset += x;
            m_data[offset] = color.a;
            break;
    }
}

Color BitmapImage::get_pixel(i32 x, i32 y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return {0, 0, 0, 0};
    }

    i32 offset = y * m_stride;
    switch (m_format) {
        case GraphicsContext::Bitmap::Format::RGBA8:
            offset += x * 4;
            return {m_data[offset], m_data[offset + 1], m_data[offset + 2], m_data[offset + 3]};
        case GraphicsContext::Bitmap::Format::RGB8:
            offset += x * 3;
            return {m_data[offset], m_data[offset + 1], m_data[offset + 2], 255};
        case GraphicsContext::Bitmap::Format::A8:
            offset += x;
            return {255, 255, 255, m_data[offset]};
    }
    return {0, 0, 0, 0};
}

void BitmapImage::fill(const Color& color) {
    fill_rect({0, 0, m_width, m_height}, color);
}

void BitmapImage::fill_rect(const RectI& rect, const Color& color) {
    i32 x0 = std::max(0, rect.x);
    i32 y0 = std::max(0, rect.y);
    i32 x1 = std::min(m_width, rect.x + rect.width);
    i32 y1 = std::min(m_height, rect.y + rect.height);

    for (i32 y = y0; y < y1; ++y) {
        for (i32 x = x0; x < x1; ++x) {
            set_pixel(x, y, color);
        }
    }
}

void BitmapImage::blit(const BitmapImage& src, i32 dest_x, i32 dest_y) {
    for (i32 y = 0; y < src.height(); ++y) {
        for (i32 x = 0; x < src.width(); ++x) {
            Color c = src.get_pixel(x, y);
            if (c.a > 0) {
                set_pixel(dest_x + x, dest_y + y, c);
            }
        }
    }
}

} // namespace lithium::platform
