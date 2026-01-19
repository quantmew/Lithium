#pragma once

#include "lithium/beryl/types.hpp"
#include "lithium/beryl/font.hpp"
#include "lithium/beryl/glyph.hpp"
#include <memory>
#include <vector>

namespace lithium::beryl {

// ============================================================================
// Text Alignment
// ============================================================================

enum class TextAlignment {
    Left,
    Right,
    Center,
    Justify
};

// ============================================================================
// Text Run
// ============================================================================

/// A contiguous run of text with the same font and style
struct TextRun {
    std::unique_ptr<Font> font;
    String text;
    usize start_index{0};      ///< Start index in source text
    usize length{0};           ///< Length of this run
    Script script{Script::Unknown};
    TextDirection direction{TextDirection::LTR};

    /// Get advance width of this run
    [[nodiscard]] f32 advance_width() const;

    /// Get number of glyphs
    [[nodiscard]] usize glyph_count() const { return length; }
};

// ============================================================================
// Glyph Position
// ============================================================================>

struct GlyphPosition {
    u32 glyph_id{0};
    Vec2 position{0, 0};       ///< Position in layout
    Vec2 advance{0, 0};        ///< Glyph advance
    f32 offset{0};             ///< Offset from baseline

    /// Character index in source text
    usize cluster_index{0};
};

// ============================================================================
// Shaped Text
// ============================================================================

/// Result of text shaping (e.g., with HarfBuzz)
struct ShapedText {
    std::vector<GlyphPosition> glyphs;
    f32 total_advance{0};
    TextDirection direction{TextDirection::LTR};
    Script script{Script::Unknown};

    /// Get number of glyphs
    [[nodiscard]] usize glyph_count() const {
        return glyphs.size();
    }

    /// Get advance width
    [[nodiscard]] f32 width() const {
        return total_advance;
    }
};

// ============================================================================
// Line
// ============================================================================

/// A single line of shaped text
struct Line {
    f32 y_position{0};          ///< Y position of baseline
    f32 ascent{0};              ///< Maximum ascent
    f32 descent{0};             ///< Maximum descent (negative)
    f32 width{0};               ///< Line width
    f32 height{0};              ///< Total line height
    std::vector<ShapedText> runs;  ///< Shaped text runs in this line

    /// Get baseline position
    [[nodiscard]] f32 baseline() const {
        return y_position + ascent;
    }

    /// Check if point is within this line
    [[nodiscard]] bool contains_y(f32 y) const {
        return y >= y_position && y < y_position + height;
    }
};

// ============================================================================
// Text Layout
// ============================================================================>

/// Complete text layout with line breaking
class TextLayout {
public:
    virtual ~TextLayout() = default;

    /// Get total size of layout
    [[nodiscard]] virtual Size size() const = 0;

    /// Get number of lines
    [[nodiscard]] virtual usize line_count() const = 0;

    /// Get a specific line
    [[nodiscard]] virtual const Line& get_line(usize index) const = 0;

    /// Hit test - find character at position
    [[nodiscard]] virtual TextHitTestResult hit_test(Vec2 position) const = 0;

    /// Get bounding box for character range
    [[nodiscard]] virtual Rect bounding_box(
        usize start_index,
        usize end_index) const = 0;

    /// Get bounding box for entire layout
    [[nodiscard]] virtual Rect bounding_box() const = 0;

    /// Get all lines
    [[nodiscard]] virtual const std::vector<Line>& lines() const = 0;

protected:
    TextLayout() = default;
};

// ============================================================================
// Text Layout Builder
// ============================================================================>

/// Configuration for text layout
struct LayoutConfig {
    f32 max_width{0};          ///< Maximum width (0 = unlimited)
    f32 max_height{0};         ///< Maximum height (0 = unlimited)
    TextDirection default_direction{TextDirection::LTR};
    TextAlignment alignment{TextAlignment::Left};
    f32 line_spacing{1.0f};    ///< Line spacing multiplier
    bool word_wrap{true};      ///< Enable word wrapping
    bool ellipsis{false};      ///< Add ellipsis for truncated text

    /// Truncation mode
    enum class Truncation {
        None,
        Character,
        Word,
        Clip
    };
    Truncation truncation{Truncation::None};

    /// String to use for ellipsis
    String ellipsis_string = "...";
};

/// Build text layouts
class TextLayoutBuilder {
public:
    virtual ~TextLayoutBuilder() = default;

    /// Set configuration
    virtual void set_config(const LayoutConfig& config) = 0;

    /// Get current configuration
    [[nodiscard]] virtual const LayoutConfig& config() const = 0;

    /// Create a layout from text with a single font
    [[nodiscard]] virtual std::unique_ptr<TextLayout> create_layout(
        const String& text,
        const FontDescription& font_desc) = 0;

    /// Create a layout from pre-shaped runs
    [[nodiscard]] virtual std::unique_ptr<TextLayout> create_layout_from_runs(
        const std::vector<TextRun>& runs) = 0;

protected:
    TextLayoutBuilder() = default;
};

/// Create a text layout builder
[[nodiscard]] std::unique_ptr<TextLayoutBuilder> create_layout_builder(
    FontManager& font_manager);

} // namespace lithium::beryl
