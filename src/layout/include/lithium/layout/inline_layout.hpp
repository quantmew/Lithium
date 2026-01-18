#pragma once

#include "box.hpp"
#include "layout_context.hpp"
#include <vector>

namespace lithium::layout {

// ============================================================================
// Line Box
// ============================================================================

struct LineFragment {
    LayoutBox* box;
    f32 x;
    f32 width;
    f32 baseline;
};

struct LineBox {
    f32 y{0};
    f32 height{0};
    f32 baseline{0};
    std::vector<LineFragment> fragments;
};

// ============================================================================
// Inline Formatting Context
// ============================================================================

class InlineFormattingContext {
public:
    InlineFormattingContext(LayoutBox& container, const LayoutContext& context);

    void run();

    [[nodiscard]] const std::vector<LineBox>& lines() const { return m_lines; }

private:
    // Line breaking
    void collect_inline_boxes(LayoutBox& box, std::vector<LayoutBox*>& boxes);
    void layout_line(std::vector<LayoutBox*>& boxes, usize start, usize end);
    void break_lines(std::vector<LayoutBox*>& boxes);

    // Text measurement
    [[nodiscard]] f32 measure_text(const String& text, const LayoutBox& box);

    // Line height calculation
    [[nodiscard]] f32 calculate_line_height(const LayoutBox& box);

    // Vertical alignment
    void align_line_vertically(LineBox& line);

    // Horizontal alignment
    void align_line_horizontally(LineBox& line, f32 available_width);

    LayoutBox& m_container;
    const LayoutContext& m_context;
    std::vector<LineBox> m_lines;

    f32 m_available_width{0};
    f32 m_current_y{0};
};

// ============================================================================
// Inline Layout Utilities
// ============================================================================

namespace inline_layout {

// Measure inline box width
[[nodiscard]] f32 measure_inline_width(LayoutBox& box, const LayoutContext& context);

// Calculate baseline
[[nodiscard]] f32 calculate_baseline(const LayoutBox& box);

// Word breaking
struct BreakOpportunity {
    usize offset;
    f32 width_before;
    bool is_forced;
};

[[nodiscard]] std::vector<BreakOpportunity> find_break_opportunities(
    const String& text, const css::ComputedValue& style);

} // namespace inline_layout

} // namespace lithium::layout
