/**
 * CSS Value implementation
 */

#include "lithium/css/value.hpp"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <unordered_map>

namespace lithium::css {

// ============================================================================
// Length
// ============================================================================

f64 Length::to_px(f64 reference_px, f64 root_font_size, f64 viewport_width, f64 viewport_height) const {
    switch (unit) {
        case LengthUnit::Px:
            return value;
        case LengthUnit::Cm:
            return value * 96.0 / 2.54;
        case LengthUnit::Mm:
            return value * 96.0 / 25.4;
        case LengthUnit::In:
            return value * 96.0;
        case LengthUnit::Pt:
            return value * 96.0 / 72.0;
        case LengthUnit::Pc:
            return value * 96.0 / 6.0;
        case LengthUnit::Q:
            return value * 96.0 / 101.6;
        case LengthUnit::Em:
            return value * reference_px;
        case LengthUnit::Rem:
            return value * root_font_size;
        case LengthUnit::Ex:
            return value * reference_px * 0.5; // Approximate
        case LengthUnit::Ch:
            return value * reference_px * 0.5; // Approximate
        case LengthUnit::Vw:
            return value * viewport_width / 100.0;
        case LengthUnit::Vh:
            return value * viewport_height / 100.0;
        case LengthUnit::Vmin:
            return value * std::min(viewport_width, viewport_height) / 100.0;
        case LengthUnit::Vmax:
            return value * std::max(viewport_width, viewport_height) / 100.0;
        case LengthUnit::Percent:
            return value * reference_px / 100.0;
    }
    return value;
}

bool Length::operator==(const Length& other) const {
    return value == other.value && unit == other.unit;
}

bool Length::operator!=(const Length& other) const {
    return !(*this == other);
}

// ============================================================================
// Angle
// ============================================================================

f64 Angle::to_degrees() const {
    switch (unit) {
        case AngleUnit::Deg:
            return value;
        case AngleUnit::Rad:
            return value * 180.0 / 3.14159265358979323846;
        case AngleUnit::Grad:
            return value * 0.9;
        case AngleUnit::Turn:
            return value * 360.0;
    }
    return value;
}

f64 Angle::to_radians() const {
    return to_degrees() * 3.14159265358979323846 / 180.0;
}

// ============================================================================
// Time
// ============================================================================

f64 Time::to_seconds() const {
    switch (unit) {
        case TimeUnit::S:
            return value;
        case TimeUnit::Ms:
            return value / 1000.0;
    }
    return value;
}

f64 Time::to_milliseconds() const {
    return to_seconds() * 1000.0;
}

// ============================================================================
// ValueParser
// ============================================================================

std::optional<Length> ValueParser::parse_length(const String& value) {
    String trimmed = value.trim();
    if (trimmed.empty()) return std::nullopt;

    // Check for 0 (unitless)
    if (trimmed == "0"_s) {
        return Length{0, LengthUnit::Px};
    }

    // Find the numeric part
    usize num_end = 0;
    bool has_dot = false;
    bool has_minus = false;

    if (trimmed[0] == '-' || trimmed[0] == '+') {
        has_minus = (trimmed[0] == '-');
        num_end = 1;
    }

    while (num_end < trimmed.length()) {
        char c = static_cast<char>(trimmed[num_end]);
        if (std::isdigit(c)) {
            num_end++;
        } else if (c == '.' && !has_dot) {
            has_dot = true;
            num_end++;
        } else {
            break;
        }
    }

    if (num_end == 0 || (num_end == 1 && (has_minus || trimmed[0] == '+'))) {
        return std::nullopt;
    }

    f64 num_value = std::strtod(trimmed.substring(0, num_end).c_str(), nullptr);
    String unit_str = trimmed.substring(num_end).to_lowercase();

    LengthUnit unit;
    if (unit_str.empty() || unit_str == "px"_s) {
        unit = LengthUnit::Px;
    } else if (unit_str == "em"_s) {
        unit = LengthUnit::Em;
    } else if (unit_str == "rem"_s) {
        unit = LengthUnit::Rem;
    } else if (unit_str == "ex"_s) {
        unit = LengthUnit::Ex;
    } else if (unit_str == "ch"_s) {
        unit = LengthUnit::Ch;
    } else if (unit_str == "vw"_s) {
        unit = LengthUnit::Vw;
    } else if (unit_str == "vh"_s) {
        unit = LengthUnit::Vh;
    } else if (unit_str == "vmin"_s) {
        unit = LengthUnit::Vmin;
    } else if (unit_str == "vmax"_s) {
        unit = LengthUnit::Vmax;
    } else if (unit_str == "%"_s) {
        unit = LengthUnit::Percent;
    } else if (unit_str == "cm"_s) {
        unit = LengthUnit::Cm;
    } else if (unit_str == "mm"_s) {
        unit = LengthUnit::Mm;
    } else if (unit_str == "in"_s) {
        unit = LengthUnit::In;
    } else if (unit_str == "pt"_s) {
        unit = LengthUnit::Pt;
    } else if (unit_str == "pc"_s) {
        unit = LengthUnit::Pc;
    } else if (unit_str == "q"_s) {
        unit = LengthUnit::Q;
    } else {
        return std::nullopt;
    }

    return Length{num_value, unit};
}

std::optional<Color> ValueParser::parse_color(const String& value) {
    String trimmed = value.trim().to_lowercase();

    // Named colors
    static const std::unordered_map<std::string, Color> named_colors = {
        {"black", Color::black()},
        {"white", Color::white()},
        {"red", Color{255, 0, 0}},
        {"green", Color{0, 128, 0}},
        {"blue", Color{0, 0, 255}},
        {"yellow", Color{255, 255, 0}},
        {"cyan", Color{0, 255, 255}},
        {"magenta", Color{255, 0, 255}},
        {"gray", Color{128, 128, 128}},
        {"grey", Color{128, 128, 128}},
        {"silver", Color{192, 192, 192}},
        {"maroon", Color{128, 0, 0}},
        {"olive", Color{128, 128, 0}},
        {"lime", Color{0, 255, 0}},
        {"aqua", Color{0, 255, 255}},
        {"teal", Color{0, 128, 128}},
        {"navy", Color{0, 0, 128}},
        {"fuchsia", Color{255, 0, 255}},
        {"purple", Color{128, 0, 128}},
        {"orange", Color{255, 165, 0}},
        {"pink", Color{255, 192, 203}},
        {"brown", Color{165, 42, 42}},
        {"transparent", Color::transparent()},
    };

    auto it = named_colors.find(std::string(trimmed.c_str()));
    if (it != named_colors.end()) {
        return it->second;
    }

    // Hex color
    if (trimmed.starts_with("#"_s)) {
        String hex = trimmed.substring(1);
        if (hex.length() == 3) {
            // #RGB -> #RRGGBB
            u8 r = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[0])).c_str(), nullptr, 16) * 17);
            u8 g = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[1])).c_str(), nullptr, 16) * 17);
            u8 b = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[2])).c_str(), nullptr, 16) * 17);
            return Color{r, g, b};
        } else if (hex.length() == 4) {
            // #RGBA
            u8 r = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[0])).c_str(), nullptr, 16) * 17);
            u8 g = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[1])).c_str(), nullptr, 16) * 17);
            u8 b = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[2])).c_str(), nullptr, 16) * 17);
            u8 a = static_cast<u8>(std::strtoul(String(1, static_cast<char>(hex[3])).c_str(), nullptr, 16) * 17);
            return Color{r, g, b, a};
        } else if (hex.length() == 6) {
            u32 rgb = static_cast<u32>(std::strtoul(hex.c_str(), nullptr, 16));
            return Color::from_rgb(rgb);
        } else if (hex.length() == 8) {
            u32 rgba = static_cast<u32>(std::strtoul(hex.c_str(), nullptr, 16));
            return Color::from_rgba(rgba);
        }
        return std::nullopt;
    }

    // rgb() / rgba()
    if (trimmed.starts_with("rgb("_s) || trimmed.starts_with("rgba("_s)) {
        usize start = trimmed.find('(');
        usize end = trimmed.find(')');
        if (start == String::npos || end == String::npos) return std::nullopt;

        String args = trimmed.substring(start + 1, end - start - 1);
        std::vector<f64> values;

        usize pos = 0;
        while (pos < args.length()) {
            // Skip whitespace and separators
            while (pos < args.length() &&
                   (args[pos] == ' ' || args[pos] == ',' || args[pos] == '/')) {
                pos++;
            }
            if (pos >= args.length()) break;

            // Parse number
            usize num_start = pos;
            while (pos < args.length() &&
                   (std::isdigit(args[pos]) || args[pos] == '.' || args[pos] == '-' || args[pos] == '%')) {
                pos++;
            }

            String num_str = args.substring(num_start, pos - num_start);
            bool is_percent = num_str.ends_with("%"_s);
            if (is_percent) {
                num_str = num_str.substring(0, num_str.length() - 1);
            }

            f64 val = std::strtod(num_str.c_str(), nullptr);
            if (is_percent) {
                val = val * 255.0 / 100.0;
            }
            values.push_back(val);
        }

        if (values.size() >= 3) {
            u8 r = static_cast<u8>(std::clamp(values[0], 0.0, 255.0));
            u8 g = static_cast<u8>(std::clamp(values[1], 0.0, 255.0));
            u8 b = static_cast<u8>(std::clamp(values[2], 0.0, 255.0));
            u8 a = values.size() >= 4 ?
                static_cast<u8>(std::clamp(values[3] * 255.0, 0.0, 255.0)) : 255;
            return Color{r, g, b, a};
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Display> ValueParser::parse_display(const String& value) {
    String lower = value.trim().to_lowercase();

    if (lower == "none"_s) return Display::None;
    if (lower == "block"_s) return Display::Block;
    if (lower == "inline"_s) return Display::Inline;
    if (lower == "inline-block"_s) return Display::InlineBlock;
    if (lower == "flex"_s) return Display::Flex;
    if (lower == "inline-flex"_s) return Display::InlineFlex;
    if (lower == "grid"_s) return Display::Grid;
    if (lower == "inline-grid"_s) return Display::InlineGrid;
    if (lower == "table"_s) return Display::Table;
    if (lower == "inline-table"_s) return Display::InlineTable;
    if (lower == "table-row"_s) return Display::TableRow;
    if (lower == "table-cell"_s) return Display::TableCell;
    if (lower == "list-item"_s) return Display::ListItem;
    if (lower == "contents"_s) return Display::Contents;

    return std::nullopt;
}

std::optional<Position> ValueParser::parse_position(const String& value) {
    String lower = value.trim().to_lowercase();

    if (lower == "static"_s) return Position::Static;
    if (lower == "relative"_s) return Position::Relative;
    if (lower == "absolute"_s) return Position::Absolute;
    if (lower == "fixed"_s) return Position::Fixed;
    if (lower == "sticky"_s) return Position::Sticky;

    return std::nullopt;
}

std::optional<FontWeight> ValueParser::parse_font_weight(const String& value) {
    String lower = value.trim().to_lowercase();

    if (lower == "normal"_s) return FontWeight::Normal;
    if (lower == "bold"_s) return FontWeight::Bold;
    if (lower == "bolder"_s) return FontWeight::Bolder;
    if (lower == "lighter"_s) return FontWeight::Lighter;
    if (lower == "100"_s) return FontWeight::W100;
    if (lower == "200"_s) return FontWeight::W200;
    if (lower == "300"_s) return FontWeight::W300;
    if (lower == "400"_s) return FontWeight::W400;
    if (lower == "500"_s) return FontWeight::W500;
    if (lower == "600"_s) return FontWeight::W600;
    if (lower == "700"_s) return FontWeight::W700;
    if (lower == "800"_s) return FontWeight::W800;
    if (lower == "900"_s) return FontWeight::W900;

    return std::nullopt;
}

bool ValueParser::apply_property(ComputedValue& style, const String& property, const String& value) {
    String prop = property.to_lowercase();
    String val = value.trim();

    // Display
    if (prop == "display"_s) {
        if (auto display = parse_display(val)) {
            style.display = *display;
            return true;
        }
        return false;
    }

    // Position
    if (prop == "position"_s) {
        if (auto position = parse_position(val)) {
            style.position = *position;
            return true;
        }
        return false;
    }

    // Color
    if (prop == "color"_s) {
        if (auto color = parse_color(val)) {
            style.color = *color;
            return true;
        }
        return false;
    }

    if (prop == "background-color"_s || prop == "background"_s) {
        if (auto color = parse_color(val)) {
            style.background_color = *color;
            return true;
        }
        return false;
    }

    // Font size
    if (prop == "font-size"_s) {
        if (auto length = parse_length(val)) {
            style.font_size = *length;
            return true;
        }
        return false;
    }

    // Font weight
    if (prop == "font-weight"_s) {
        if (auto weight = parse_font_weight(val)) {
            style.font_weight = *weight;
            return true;
        }
        return false;
    }

    // Width/Height
    if (prop == "width"_s) {
        if (val == "auto"_s) {
            style.width = std::nullopt;
            return true;
        }
        if (auto length = parse_length(val)) {
            style.width = *length;
            return true;
        }
        return false;
    }

    if (prop == "height"_s) {
        if (val == "auto"_s) {
            style.height = std::nullopt;
            return true;
        }
        if (auto length = parse_length(val)) {
            style.height = *length;
            return true;
        }
        return false;
    }

    // Margin
    if (prop == "margin"_s) {
        if (auto length = parse_length(val)) {
            style.margin_top = *length;
            style.margin_right = *length;
            style.margin_bottom = *length;
            style.margin_left = *length;
            return true;
        }
        return false;
    }

    if (prop == "margin-top"_s) {
        if (auto length = parse_length(val)) {
            style.margin_top = *length;
            return true;
        }
        return false;
    }

    if (prop == "margin-right"_s) {
        if (auto length = parse_length(val)) {
            style.margin_right = *length;
            return true;
        }
        return false;
    }

    if (prop == "margin-bottom"_s) {
        if (auto length = parse_length(val)) {
            style.margin_bottom = *length;
            return true;
        }
        return false;
    }

    if (prop == "margin-left"_s) {
        if (auto length = parse_length(val)) {
            style.margin_left = *length;
            return true;
        }
        return false;
    }

    // Padding
    if (prop == "padding"_s) {
        if (auto length = parse_length(val)) {
            style.padding_top = *length;
            style.padding_right = *length;
            style.padding_bottom = *length;
            style.padding_left = *length;
            return true;
        }
        return false;
    }

    if (prop == "padding-top"_s) {
        if (auto length = parse_length(val)) {
            style.padding_top = *length;
            return true;
        }
        return false;
    }

    if (prop == "padding-right"_s) {
        if (auto length = parse_length(val)) {
            style.padding_right = *length;
            return true;
        }
        return false;
    }

    if (prop == "padding-bottom"_s) {
        if (auto length = parse_length(val)) {
            style.padding_bottom = *length;
            return true;
        }
        return false;
    }

    if (prop == "padding-left"_s) {
        if (auto length = parse_length(val)) {
            style.padding_left = *length;
            return true;
        }
        return false;
    }

    // Border width
    if (prop == "border-width"_s) {
        if (auto length = parse_length(val)) {
            style.border_top_width = *length;
            style.border_right_width = *length;
            style.border_bottom_width = *length;
            style.border_left_width = *length;
            return true;
        }
        return false;
    }

    // Opacity
    if (prop == "opacity"_s) {
        f64 opacity = std::strtod(val.c_str(), nullptr);
        style.opacity = static_cast<f32>(std::clamp(opacity, 0.0, 1.0));
        return true;
    }

    // Text align
    if (prop == "text-align"_s) {
        String lower = val.to_lowercase();
        if (lower == "left"_s) { style.text_align = TextAlign::Left; return true; }
        if (lower == "right"_s) { style.text_align = TextAlign::Right; return true; }
        if (lower == "center"_s) { style.text_align = TextAlign::Center; return true; }
        if (lower == "justify"_s) { style.text_align = TextAlign::Justify; return true; }
        if (lower == "start"_s) { style.text_align = TextAlign::Start; return true; }
        if (lower == "end"_s) { style.text_align = TextAlign::End; return true; }
        return false;
    }

    // Line height
    if (prop == "line-height"_s) {
        if (val == "normal"_s) {
            style.line_height = Length{1.2f, LengthUnit::Em};
            return true;
        }
        if (auto length = parse_length(val)) {
            style.line_height = *length;
            return true;
        }
        // Unitless number
        f64 multiplier = std::strtod(val.c_str(), nullptr);
        if (multiplier > 0) {
            style.line_height = Length{multiplier, LengthUnit::Em};
            return true;
        }
        return false;
    }

    return false;
}

} // namespace lithium::css
