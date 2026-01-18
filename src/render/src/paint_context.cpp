/**
 * Paint Context implementation (simplified)
 */

#include "lithium/render/paint_context.hpp"
#include <type_traits>

namespace lithium::render {

PaintContext::PaintContext(platform::GraphicsContext& graphics, text::FontContext& fonts)
    : m_graphics(graphics)
    , m_fonts(fonts) {}

void PaintContext::execute(const DisplayList& display_list) {
    for (const auto& cmd : display_list.commands()) {
        execute_command(cmd);
    }
}

void PaintContext::fill_rect(const RectF& rect, const Color& color) {
    m_graphics.fill_rect(rect, color);
}

void PaintContext::stroke_rect(const RectF& rect, const Color& color, f32 width) {
    m_graphics.stroke_rect(rect, color, width);
}

void PaintContext::draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) {
    m_graphics.draw_line(from, to, color, width);
}

void PaintContext::draw_text(const PointF& position, const String& text,
                             const String& /*font_family*/, f32 font_size, const Color& color) {
    m_graphics.draw_text(position, text, color, font_size);
}

void PaintContext::push_clip(const RectF& rect) {
    m_clip_stack.push(rect);
    m_graphics.push_clip(rect);
}

void PaintContext::pop_clip() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop();
    }
    m_graphics.pop_clip();
}

void PaintContext::push_opacity(f32 opacity) {
    m_opacity_stack.push(m_current_opacity);
    m_current_opacity *= opacity;
    m_graphics.push_opacity(opacity);
}

void PaintContext::pop_opacity() {
    if (!m_opacity_stack.empty()) {
        m_current_opacity = m_opacity_stack.top();
        m_opacity_stack.pop();
    }
    m_graphics.pop_opacity();
}

void PaintContext::push_transform(f32 tx, f32 ty, f32 sx, f32 sy, f32 rotate) {
    m_transform_stack.push({tx, ty, sx, sy, rotate});
    m_graphics.push_transform();
    m_graphics.translate(tx, ty);
    m_graphics.scale(sx, sy);
    m_graphics.rotate(rotate);
}

void PaintContext::pop_transform() {
    if (!m_transform_stack.empty()) {
        m_transform_stack.pop();
    }
    m_graphics.pop_transform();
}

RectF PaintContext::viewport() const {
    auto size = m_graphics.viewport_size();
    return {0, 0, static_cast<f32>(size.width), static_cast<f32>(size.height)};
}

void PaintContext::execute_command(const DisplayCommand& cmd) {
    std::visit([this](auto&& command) {
        using T = std::decay_t<decltype(command)>;
        if constexpr (std::is_same_v<T, FillRectCommand>) {
            fill_rect(command.rect, command.color);
        } else if constexpr (std::is_same_v<T, StrokeRectCommand>) {
            stroke_rect(command.rect, command.color, command.width);
        } else if constexpr (std::is_same_v<T, DrawLineCommand>) {
            draw_line(command.from, command.to, command.color, command.width);
        } else if constexpr (std::is_same_v<T, DrawTextCommand>) {
            draw_text(command.position, command.text, command.font_family, command.font_size, command.color);
        } else if constexpr (std::is_same_v<T, DrawImageCommand>) {
            // Image drawing stub
            stroke_rect(command.dest, {128, 128, 128, 255}, 1.0f);
        } else if constexpr (std::is_same_v<T, PushClipCommand>) {
            push_clip(command.rect);
        } else if constexpr (std::is_same_v<T, PopClipCommand>) {
            pop_clip();
        } else if constexpr (std::is_same_v<T, PushOpacityCommand>) {
            push_opacity(command.opacity);
        } else if constexpr (std::is_same_v<T, PopOpacityCommand>) {
            pop_opacity();
        } else if constexpr (std::is_same_v<T, PushTransformCommand>) {
            push_transform(command.translate_x, command.translate_y,
                           command.scale_x, command.scale_y, command.rotate);
        } else if constexpr (std::is_same_v<T, PopTransformCommand>) {
            pop_transform();
        }
    }, cmd);
}

void paint_display_list(platform::GraphicsContext& graphics,
                        text::FontContext& fonts,
                        const DisplayList& display_list) {
    PaintContext context(graphics, fonts);
    context.execute(display_list);
}

void paint_display_list(platform::GraphicsContext& graphics,
                        const DisplayList& display_list) {
    static text::FontContext fonts;
    paint_display_list(graphics, fonts, display_list);
}

} // namespace lithium::render
