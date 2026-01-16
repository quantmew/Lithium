/**
 * Paint Context implementation
 */

#include "lithium/render/paint_context.hpp"
#include "lithium/render/display_list.hpp"
#include "lithium/platform/graphics_context.hpp"

namespace lithium::render {

// ============================================================================
// Paint Context
// ============================================================================

class PaintContext {
public:
    explicit PaintContext(platform::GraphicsContext& graphics)
        : m_graphics(graphics)
    {}

    void paint(const DisplayList& display_list) {
        for (const auto& cmd : display_list) {
            std::visit([this](auto&& command) {
                paint_command(command);
            }, cmd);
        }
    }

private:
    void paint_command(const FillRectCommand& cmd) {
        m_graphics.fill_rect(cmd.rect, cmd.color);
    }

    void paint_command(const StrokeRectCommand& cmd) {
        m_graphics.stroke_rect(cmd.rect, cmd.color, cmd.width);
    }

    void paint_command(const DrawLineCommand& cmd) {
        m_graphics.draw_line(cmd.from, cmd.to, cmd.color, cmd.width);
    }

    void paint_command(const DrawTextCommand& cmd) {
        m_graphics.draw_text(cmd.position, cmd.text, cmd.color, cmd.font_size);
    }

    void paint_command(const DrawImageCommand& cmd) {
        // Image loading would be handled by resource loader
        // For now, just draw a placeholder rect
        m_graphics.stroke_rect(cmd.dest, {128, 128, 128, 255}, 1.0f);
    }

    void paint_command(const PushClipCommand& cmd) {
        m_graphics.push_clip(cmd.rect);
    }

    void paint_command(const PopClipCommand&) {
        m_graphics.pop_clip();
    }

    void paint_command(const PushOpacityCommand& cmd) {
        m_graphics.push_opacity(cmd.opacity);
    }

    void paint_command(const PopOpacityCommand&) {
        m_graphics.pop_opacity();
    }

    void paint_command(const PushTransformCommand& cmd) {
        m_graphics.push_transform();
        m_graphics.translate(cmd.translate_x, cmd.translate_y);
        m_graphics.scale(cmd.scale_x, cmd.scale_y);
        m_graphics.rotate(cmd.rotate);
    }

    void paint_command(const PopTransformCommand&) {
        m_graphics.pop_transform();
    }

    platform::GraphicsContext& m_graphics;
};

// ============================================================================
// Public function
// ============================================================================

void paint_display_list(platform::GraphicsContext& graphics, const DisplayList& display_list) {
    PaintContext context(graphics);
    context.paint(display_list);
}

} // namespace lithium::render
