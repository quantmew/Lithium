#pragma once

#include "display_list.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/text/font.hpp"
#include <stack>

namespace lithium::render {

// ============================================================================
// Paint Context - Executes display list on a graphics context
// ============================================================================

class PaintContext {
public:
    PaintContext(platform::GraphicsContext& graphics, text::FontContext& fonts);

    // Execute a display list
    void execute(const DisplayList& display_list);

    // Direct drawing (for immediate mode rendering)
    void fill_rect(const RectF& rect, const Color& color);
    void stroke_rect(const RectF& rect, const Color& color, f32 width);
    void draw_line(const PointF& from, const PointF& to, const Color& color, f32 width);
    void draw_text(const PointF& position, const String& text,
                   const String& font_family, f32 font_size, const Color& color);

    // State management
    void push_clip(const RectF& rect);
    void pop_clip();
    void push_opacity(f32 opacity);
    void pop_opacity();
    void push_transform(f32 tx, f32 ty, f32 sx, f32 sy, f32 rotate);
    void pop_transform();

    // Viewport
    [[nodiscard]] RectF viewport() const;

private:
    void execute_command(const DisplayCommand& cmd);

    platform::GraphicsContext& m_graphics;
    text::FontContext& m_fonts;

    // State stacks
    std::stack<RectF> m_clip_stack;
    std::stack<f32> m_opacity_stack;

    struct Transform {
        f32 translate_x{0};
        f32 translate_y{0};
        f32 scale_x{1};
        f32 scale_y{1};
        f32 rotate{0};
    };
    std::stack<Transform> m_transform_stack;

    f32 m_current_opacity{1.0f};
};

// ============================================================================
// Layer - For compositing
// ============================================================================

class Layer {
public:
    Layer(i32 width, i32 height);

    [[nodiscard]] i32 width() const { return m_width; }
    [[nodiscard]] i32 height() const { return m_height; }

    // Render content to this layer
    void begin();
    void end();

    // Get the layer content as a bitmap
    [[nodiscard]] const platform::BitmapImage& content() const { return m_content; }

    // Opacity
    void set_opacity(f32 opacity) { m_opacity = opacity; }
    [[nodiscard]] f32 opacity() const { return m_opacity; }

private:
    i32 m_width;
    i32 m_height;
    f32 m_opacity{1.0f};
    platform::BitmapImage m_content;
};

} // namespace lithium::render
