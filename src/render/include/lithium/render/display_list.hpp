#pragma once

#include "lithium/core/types.hpp"
#include "lithium/layout/box.hpp"
#include <variant>
#include <vector>

namespace lithium::render {

// ============================================================================
// Display Commands
// ============================================================================

struct FillRectCommand {
    RectF rect;
    Color color;
};

struct StrokeRectCommand {
    RectF rect;
    Color color;
    f32 width;
};

struct DrawLineCommand {
    PointF from;
    PointF to;
    Color color;
    f32 width;
};

struct DrawTextCommand {
    PointF position;
    String text;
    String font_family;
    f32 font_size;
    Color color;
};

struct DrawImageCommand {
    RectF dest;
    RectF src;
    String image_url;
};

struct PushClipCommand {
    RectF rect;
};

struct PopClipCommand {};

struct PushOpacityCommand {
    f32 opacity;
};

struct PopOpacityCommand {};

struct PushTransformCommand {
    f32 translate_x{0};
    f32 translate_y{0};
    f32 scale_x{1};
    f32 scale_y{1};
    f32 rotate{0};  // radians
};

struct PopTransformCommand {};

using DisplayCommand = std::variant<
    FillRectCommand,
    StrokeRectCommand,
    DrawLineCommand,
    DrawTextCommand,
    DrawImageCommand,
    PushClipCommand,
    PopClipCommand,
    PushOpacityCommand,
    PopOpacityCommand,
    PushTransformCommand,
    PopTransformCommand
>;

// ============================================================================
// Display List
// ============================================================================

class DisplayList {
public:
    DisplayList() = default;

    // Add commands
    void push(const DisplayCommand& cmd) { m_commands.push_back(cmd); }

    template<typename T>
    void push(T&& cmd) {
        m_commands.push_back(std::forward<T>(cmd));
    }

    // Access
    [[nodiscard]] const std::vector<DisplayCommand>& commands() const { return m_commands; }
    [[nodiscard]] usize size() const { return m_commands.size(); }
    [[nodiscard]] bool empty() const { return m_commands.empty(); }

    // Clear
    void clear() { m_commands.clear(); }

    // Iteration
    auto begin() const { return m_commands.begin(); }
    auto end() const { return m_commands.end(); }

    // Optimization
    void optimize();

private:
    std::vector<DisplayCommand> m_commands;
};

// ============================================================================
// Display List Builder
// ============================================================================

class DisplayListBuilder {
public:
    DisplayListBuilder();

    // Build display list from layout tree
    [[nodiscard]] DisplayList build(const layout::LayoutBox& root);

private:
    void paint_box(const layout::LayoutBox& box);
    void paint_background(const layout::LayoutBox& box);
    void paint_borders(const layout::LayoutBox& box);
    void paint_text(const layout::LayoutBox& box);
    void paint_children(const layout::LayoutBox& box);

    // Border painting helpers
    void paint_border_top(const layout::LayoutBox& box);
    void paint_border_right(const layout::LayoutBox& box);
    void paint_border_bottom(const layout::LayoutBox& box);
    void paint_border_left(const layout::LayoutBox& box);

    DisplayList m_display_list;
};

} // namespace lithium::render
