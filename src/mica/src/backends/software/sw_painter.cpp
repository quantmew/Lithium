/**
 * Software Painter Implementation
 */

#include "sw_backend.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace lithium::mica::software {

// ============================================================================
// SoftwarePainter
// ============================================================================

SoftwarePainter::SoftwarePainter(SoftwareContext& context)
    : m_context(context)
{
    m_current_state.transform = Mat3::identity();
    m_current_state.line_width = 1.0f;
}

SoftwarePainter::~SoftwarePainter() = default;

Context* SoftwarePainter::context() noexcept {
    return &m_context;
}

void SoftwarePainter::save() {
    m_state_stack.push_back(std::make_unique<PainterState>(std::move(m_current_state)));
    // Reset current state to default after moving
    m_current_state.transform = Mat3::identity();
    m_current_state.line_width = 1.0f;
}

void SoftwarePainter::restore() {
    if (!m_state_stack.empty()) {
        m_current_state = std::move(*m_state_stack.back());
        m_state_stack.pop_back();
    }
}

const PainterState& SoftwarePainter::state() const noexcept {
    return m_current_state;
}

void SoftwarePainter::translate(Vec2 offset) {
    Mat3 translation = Mat3::translation(offset.x, offset.y);
    m_current_state.transform = translation * m_current_state.transform;
}

void SoftwarePainter::scale(Vec2 factors) {
    Mat3 scaling = Mat3::scale(factors.x, factors.y);
    m_current_state.transform = scaling * m_current_state.transform;
}

void SoftwarePainter::rotate(f32 angle) {
    Mat3 rotation = Mat3::rotation(angle);
    m_current_state.transform = rotation * m_current_state.transform;
}

void SoftwarePainter::concat(const Mat3& matrix) {
    m_current_state.transform = matrix * m_current_state.transform;
}

void SoftwarePainter::set_transform(const Mat3& transform) {
    m_current_state.transform = transform;
}

const Mat3& SoftwarePainter::transform() const noexcept {
    return m_current_state.transform;
}

void SoftwarePainter::draw_line(Vec2 start, Vec2 end, const Paint& paint) {
    // Get color
    Color color;
    if (paint.brush && paint.brush->type() == BrushType::Solid) {
        auto* solid = static_cast<SolidBrush*>(paint.brush.get());
        color = solid->color;
        color.a *= paint.opacity;
    } else {
        color = Color{1, 1, 1, paint.opacity};
    }

    // Apply transform to points
    Vec3 p1 = m_current_state.transform * Vec3{start.x, start.y, 1.0f};
    Vec3 p2 = m_current_state.transform * Vec3{end.x, end.y, 1.0f};

    // Bresenham's line algorithm
    i32 x1 = static_cast<i32>(p1.x);
    i32 y1 = static_cast<i32>(p1.y);
    i32 x2 = static_cast<i32>(p2.x);
    i32 y2 = static_cast<i32>(p2.y);

    i32 dx = std::abs(x2 - x1);
    i32 dy = std::abs(y2 - y1);
    i32 sx = (x1 < x2) ? 1 : -1;
    i32 sy = (y1 < y2) ? 1 : -1;
    i32 err = dx - dy;

    while (true) {
        set_pixel(x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        i32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void SoftwarePainter::fill_rect(const Rect& rect, const Paint& paint) {
    // Get color
    Color color;
    if (paint.brush && paint.brush->type() == BrushType::Solid) {
        auto* solid = static_cast<SolidBrush*>(paint.brush.get());
        color = solid->color;
        color.a *= paint.opacity;
    } else {
        color = Color{1, 1, 1, paint.opacity};
    }

    // Get bounds
    i32 x1 = static_cast<i32>(rect.x);
    i32 y1 = static_cast<i32>(rect.y);
    i32 x2 = static_cast<i32>(rect.x + rect.width);
    i32 y2 = static_cast<i32>(rect.y + rect.height);

    // Clip to frame buffer
    i32 fb_width = m_context.size().width;
    i32 fb_height = m_context.size().height;

    x1 = std::max(0, std::min(x1, fb_width));
    y1 = std::max(0, std::min(y1, fb_height));
    x2 = std::max(0, std::min(x2, fb_width));
    y2 = std::max(0, std::min(y2, fb_height));

    // Fill rectangle
    for (i32 y = y1; y < y2; ++y) {
        for (i32 x = x1; x < x2; ++x) {
            set_pixel(x, y, color);
        }
    }
}

void SoftwarePainter::draw_rect(const Rect& rect, const Paint& paint) {
    draw_line({rect.x, rect.y}, {rect.x + rect.width, rect.y}, paint);
    draw_line({rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + rect.height}, paint);
    draw_line({rect.x + rect.width, rect.y + rect.height}, {rect.x, rect.y + rect.height}, paint);
    draw_line({rect.x, rect.y + rect.height}, {rect.x, rect.y}, paint);
}

void SoftwarePainter::draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    // Simplified: just draw regular rectangle
    draw_rect(rect, paint);
}

void SoftwarePainter::fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    // Simplified: just fill regular rectangle
    fill_rect(rect, paint);
}

void SoftwarePainter::draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    // TODO: Implement ellipse drawing
    std::cerr << "SoftwarePainter: Ellipse drawing not yet implemented" << std::endl;
}

void SoftwarePainter::fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    // TODO: Implement ellipse filling
    std::cerr << "SoftwarePainter: Ellipse filling not yet implemented" << std::endl;
}

void SoftwarePainter::draw_circle(Vec2 center, f32 radius, const Paint& paint) {
    draw_ellipse(center, radius, radius, paint);
}

void SoftwarePainter::fill_circle(Vec2 center, f32 radius, const Paint& paint) {
    fill_ellipse(center, radius, radius, paint);
}

void SoftwarePainter::draw_path(const Path& path, const Paint& paint) {
    // TODO: Implement path drawing
    std::cerr << "SoftwarePainter: Path drawing not yet implemented" << std::endl;
}

void SoftwarePainter::fill_path(const Path& path, const Paint& paint) {
    // TODO: Implement path filling
    std::cerr << "SoftwarePainter: Path filling not yet implemented" << std::endl;
}

void SoftwarePainter::draw_text(
    Vec2 position,
    const String& text,
    const Paint& paint,
    const beryl::FontDescription& font_desc) {

    // TODO: Implement text rendering
    std::cerr << "SoftwarePainter: Text rendering not yet implemented" << std::endl;
}

void SoftwarePainter::draw_text_layout(
    Vec2 position,
    const beryl::TextLayout& layout,
    const Paint& paint) {

    // TODO: Implement text layout rendering
    std::cerr << "SoftwarePainter: Text layout rendering not yet implemented" << std::endl;
}

void SoftwarePainter::draw_image(Vec2 position, Texture* texture, const Paint& paint) {
    // TODO: Implement image rendering
    std::cerr << "SoftwarePainter: Image rendering not yet implemented" << std::endl;
}

void SoftwarePainter::draw_image_rect(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Paint& paint) {

    // TODO: Implement image rendering
    std::cerr << "SoftwarePainter: Image rendering not yet implemented" << std::endl;
}

void SoftwarePainter::draw_image_tinted(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Color& tint) {

    // TODO: Implement tinted image rendering
    std::cerr << "SoftwarePainter: Tinted image rendering not yet implemented" << std::endl;
}

void SoftwarePainter::clip_rect(const Rect& rect) {
    // TODO: Implement clipping
    std::cerr << "SoftwarePainter: Rect clipping not yet implemented" << std::endl;
}

void SoftwarePainter::clip_path(const Path& path) {
    // TODO: Implement path clipping
    std::cerr << "SoftwarePainter: Path clipping not yet implemented" << std::endl;
}

void SoftwarePainter::reset_clip() {
    // TODO: Reset clipping
}

void SoftwarePainter::clear(const Color& color) {
    u32 pixel = color.to_u32();
    usize buffer_size = static_cast<usize>(m_context.size().width) * static_cast<usize>(m_context.size().height);
    std::fill(m_context.frame_buffer(), m_context.frame_buffer() + buffer_size, pixel);
}

// ============================================================================
// Helper Functions
// ============================================================================>

void SoftwarePainter::set_pixel(i32 x, i32 y, const Color& color) {
    i32 width = m_context.size().width;
    i32 height = m_context.size().height;

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    usize index = static_cast<usize>(y * width + x);
    m_context.frame_buffer()[index] = color.to_u32();
}

Color SoftwarePainter::blend_pixel(i32 x, i32 y, const Color& color) {
    i32 width = m_context.size().width;
    i32 height = m_context.size().height;

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return color;
    }

    usize index = static_cast<usize>(y * width + x);
    u32 existing = m_context.frame_buffer()[index];

    // Convert to RGBA
    f32 dst_r = ((existing >> 16) & 0xFF) / 255.0f;
    f32 dst_g = ((existing >> 8) & 0xFF) / 255.0f;
    f32 dst_b = (existing & 0xFF) / 255.0f;
    f32 dst_a = ((existing >> 24) & 0xFF) / 255.0f;

    // Alpha blending
    f32 result_a = color.a + dst_a * (1.0f - color.a);
    f32 result_r = (color.r * color.a + dst_r * dst_a * (1.0f - color.a)) / result_a;
    f32 result_g = (color.g * color.a + dst_g * dst_a * (1.0f - color.a)) / result_a;
    f32 result_b = (color.b * color.a + dst_b * dst_a * (1.0f - color.a)) / result_a;

    Color result{result_r, result_g, result_b, result_a};
    m_context.frame_buffer()[index] = result.to_u32();

    return result;
}

void SoftwarePainter::draw_h_line(i32 x1, i32 x2, i32 y, const Color& color) {
    i32 width = m_context.size().width;

    if (y < 0 || y >= m_context.size().height) {
        return;
    }

    x1 = std::max(0, x1);
    x2 = std::min(width, x2);

    for (i32 x = x1; x < x2; ++x) {
        set_pixel(x, y, color);
    }
}

void SoftwarePainter::draw_v_line(i32 x, i32 y1, i32 y2, const Color& color) {
    i32 height = m_context.size().height;

    if (x < 0 || x >= m_context.size().width) {
        return;
    }

    y1 = std::max(0, y1);
    y2 = std::min(height, y2);

    for (i32 y = y1; y < y2; ++y) {
        set_pixel(x, y, color);
    }
}

} // namespace lithium::mica::software
