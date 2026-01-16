#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>
#include <memory>

namespace lithium::css {

// ============================================================================
// Component Values (CSS Syntax Module Level 3)
// ============================================================================

struct SimpleBlock;
struct Function;

struct PreservedToken {
    String value;  // Simplified - in full impl would store actual token
};

using ComponentValue = std::variant<
    PreservedToken,
    std::shared_ptr<SimpleBlock>,
    std::shared_ptr<Function>
>;

struct SimpleBlock {
    unicode::CodePoint associated_token;  // {, [, or (
    std::vector<ComponentValue> value;
};

struct Function {
    String name;
    std::vector<ComponentValue> value;
};

// ============================================================================
// CSS Value Types
// ============================================================================

// Length units
enum class LengthUnit {
    // Absolute lengths
    Px, Cm, Mm, In, Pt, Pc, Q,
    // Relative lengths
    Em, Rem, Ex, Ch, Vw, Vh, Vmin, Vmax,
    // Percentage (treated as length in some contexts)
    Percent
};

struct Length {
    f64 value{0};
    LengthUnit unit{LengthUnit::Px};

    [[nodiscard]] f64 to_px(f64 reference_px = 0, f64 root_font_size = 16, f64 viewport_width = 0, f64 viewport_height = 0) const;

    [[nodiscard]] bool operator==(const Length& other) const;
    [[nodiscard]] bool operator!=(const Length& other) const;
};

// Angle units
enum class AngleUnit { Deg, Rad, Grad, Turn };

struct Angle {
    f64 value{0};
    AngleUnit unit{AngleUnit::Deg};

    [[nodiscard]] f64 to_degrees() const;
    [[nodiscard]] f64 to_radians() const;
};

// Time units
enum class TimeUnit { S, Ms };

struct Time {
    f64 value{0};
    TimeUnit unit{TimeUnit::S};

    [[nodiscard]] f64 to_seconds() const;
    [[nodiscard]] f64 to_milliseconds() const;
};

// ============================================================================
// CSS Property Values
// ============================================================================

// Display
enum class Display {
    None, Block, Inline, InlineBlock, Flex, InlineFlex, Grid, InlineGrid,
    Table, InlineTable, TableRow, TableCell, ListItem, Contents
};

// Position
enum class Position {
    Static, Relative, Absolute, Fixed, Sticky
};

// Float
enum class Float {
    None, Left, Right
};

// Clear
enum class Clear {
    None, Left, Right, Both
};

// Overflow
enum class Overflow {
    Visible, Hidden, Scroll, Auto
};

// Visibility
enum class Visibility {
    Visible, Hidden, Collapse
};

// Text alignment
enum class TextAlign {
    Left, Right, Center, Justify, Start, End
};

// Vertical alignment
enum class VerticalAlign {
    Baseline, Sub, Super, TextTop, TextBottom, Middle, Top, Bottom
};

// Font weight
enum class FontWeight {
    Normal,    // 400
    Bold,      // 700
    Bolder,
    Lighter,
    W100, W200, W300, W400, W500, W600, W700, W800, W900
};

// Font style
enum class FontStyle {
    Normal, Italic, Oblique
};

// White space
enum class WhiteSpace {
    Normal, Pre, Nowrap, PreWrap, PreLine, BreakSpaces
};

// Text decoration line
enum class TextDecorationLine {
    None, Underline, Overline, LineThrough
};

// Box sizing
enum class BoxSizing {
    ContentBox, BorderBox
};

// Border style
enum class BorderStyle {
    None, Hidden, Dotted, Dashed, Solid, Double, Groove, Ridge, Inset, Outset
};

// ============================================================================
// Computed Value
// ============================================================================

struct ComputedValue {
    // Layout
    Display display{Display::Inline};
    Position position{Position::Static};
    Float float_value{Float::None};
    Clear clear{Clear::None};
    Overflow overflow_x{Overflow::Visible};
    Overflow overflow_y{Overflow::Visible};
    Visibility visibility{Visibility::Visible};
    BoxSizing box_sizing{BoxSizing::ContentBox};

    // Box model
    std::optional<Length> width;
    std::optional<Length> height;
    std::optional<Length> min_width;
    std::optional<Length> min_height;
    std::optional<Length> max_width;
    std::optional<Length> max_height;

    Length margin_top;
    Length margin_right;
    Length margin_bottom;
    Length margin_left;

    Length padding_top;
    Length padding_right;
    Length padding_bottom;
    Length padding_left;

    Length border_top_width;
    Length border_right_width;
    Length border_bottom_width;
    Length border_left_width;

    BorderStyle border_top_style{BorderStyle::None};
    BorderStyle border_right_style{BorderStyle::None};
    BorderStyle border_bottom_style{BorderStyle::None};
    BorderStyle border_left_style{BorderStyle::None};

    Color border_top_color{Color::black()};
    Color border_right_color{Color::black()};
    Color border_bottom_color{Color::black()};
    Color border_left_color{Color::black()};

    // Positioning
    std::optional<Length> top;
    std::optional<Length> right;
    std::optional<Length> bottom;
    std::optional<Length> left;
    i32 z_index{0};

    // Text
    Color color{Color::black()};
    TextAlign text_align{TextAlign::Start};
    VerticalAlign vertical_align{VerticalAlign::Baseline};
    WhiteSpace white_space{WhiteSpace::Normal};
    TextDecorationLine text_decoration_line{TextDecorationLine::None};
    Color text_decoration_color{Color::black()};

    // Font
    Length font_size{16, LengthUnit::Px};
    FontWeight font_weight{FontWeight::Normal};
    FontStyle font_style{FontStyle::Normal};
    std::vector<String> font_family;
    Length line_height{1.2f, LengthUnit::Em};

    // Background
    Color background_color{Color::transparent()};

    // Opacity
    f32 opacity{1.0f};
};

// ============================================================================
// Value Parsing
// ============================================================================

class ValueParser {
public:
    // Parse specific value types
    [[nodiscard]] static std::optional<Length> parse_length(const String& value);
    [[nodiscard]] static std::optional<Color> parse_color(const String& value);
    [[nodiscard]] static std::optional<Display> parse_display(const String& value);
    [[nodiscard]] static std::optional<Position> parse_position(const String& value);
    [[nodiscard]] static std::optional<FontWeight> parse_font_weight(const String& value);

    // Parse a property value
    [[nodiscard]] static bool apply_property(ComputedValue& style, const String& property, const String& value);
};

} // namespace lithium::css
