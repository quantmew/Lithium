#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <cstdint>
#include <vector>
#include <functional>

namespace lithium::beryl {

// ============================================================================
// Basic Types
// ============================================================================

using u8 = lithium::u8;
using u16 = lithium::u16;
using u32 = lithium::u32;
using u64 = lithium::u64;
using i8 = lithium::i8;
using i16 = lithium::i16;
using i32 = lithium::i32;
using i64 = lithium::i64;
using f32 = lithium::f32;
using f64 = lithium::f64;

using String = lithium::String;
using Vec2 = lithium::PointF;
using Rect = lithium::RectF;
using Size = lithium::SizeF;

// ============================================================================
// Unicode Code Point
// ============================================================================

using CodePoint = u32;

/// Special code points
constexpr CodePoint INVALID_CODEPOINT = 0xFFFFFFFF;
constexpr CodePoint REPLACEMENT_CHARACTER = 0xFFFD;

// ============================================================================
// Font Properties
// ============================================================================

enum class FontWeight {
    Thin = 100,
    ExtraLight = 200,
    Light = 300,
    Normal = 400,
    Medium = 500,
    SemiBold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900
};

enum class FontStyle {
    Normal,
    Italic,
    Oblique
};

enum class FontStretch {
    UltraCondensed = 1,
    ExtraCondensed = 2,
    Condensed = 3,
    SemiCondensed = 4,
    Normal = 5,
    SemiExpanded = 6,
    Expanded = 7,
    ExtraExpanded = 8,
    UltraExpanded = 9
};

/// Font description for matching and loading fonts
struct FontDescription {
    String family;           ///< Font family name (e.g., "Arial", "Segoe UI")
    f32 size{12.0f};         ///< Font size in points
    FontWeight weight{FontWeight::Normal};
    FontStyle style{FontStyle::Normal};
    FontStretch stretch{FontStretch::Normal};

    /// Check equality
    [[nodiscard]] bool operator==(const FontDescription& other) const {
        return family == other.family &&
               size == other.size &&
               weight == other.weight &&
               style == other.style &&
               stretch == other.stretch;
    }

    [[nodiscard]] bool operator!=(const FontDescription& other) const {
        return !(*this == other);
    }

    /// Create a hash for use in unordered_map
    [[nodiscard]] usize hash() const;
};

} // namespace lithium::beryl

// Hash support for FontDescription - must be outside the namespace
template<>
struct std::hash<lithium::beryl::FontDescription> {
    lithium::usize operator()(const lithium::beryl::FontDescription& desc) const noexcept {
        return desc.hash();
    }
};

namespace lithium::beryl {

// ============================================================================
// Font Metrics
// ============================================================================

struct FontMetrics {
    f32 ascent{0};          ///< Distance from baseline to top of ascenders
    f32 descent{0};         ///< Distance from baseline to bottom of descenders (negative)
    f32 line_gap{0};        ///< Extra spacing between lines
    f32 cap_height{0};      ///< Height of capital letters
    f32 x_height{0};        ///< Height of lowercase 'x'
    f32 units_per_em{0};    ///< Font design units per EM

    /// Total line height
    [[nodiscard]] f32 line_height() const {
        return ascent - descent + line_gap;
    }

    /// Height above baseline
    [[nodiscard]] f32 height_above_baseline() const {
        return ascent;
    }

    /// Height below baseline (positive value)
    [[nodiscard]] f32 height_below_baseline() const {
        return -descent;
    }
};

// ============================================================================
// Glyph Metrics
// ============================================================================

struct GlyphMetrics {
    u32 glyph_id{0};        ///< Glyph index in font
    Vec2 bearing{0, 0};     ///< Offset from origin to glyph bounding box
    Vec2 advance{0, 0};     ///< Advance width/height
    Size size{0, 0};        ///< Bounding box size
};

// ============================================================================
// Text Rendering Quality
// ============================================================================

enum class TextAntialiasing {
    None,           ///< No antialiasing
    Grayscale,      ///< Grayscale antialiasing
    Subpixel,       ///< Subpixel (LCD) antialiasing
    Default         ///< Use system default
};

enum class TextRenderingMode {
    Auto,           ///< Automatically choose best mode
    Monochrome,     ///< Monochrome rendering
    Antialiased,    ///< Grayscale antialiasing
    Subpixel,       ///< Subpixel (LCD) rendering
    Bitmap          ///< Use embedded bitmaps
};

// ============================================================================
// Text Direction and Script
// ============================================================================

enum class TextDirection {
    LTR,            ///< Left-to-right (e.g., English)
    RTL,            ///< Right-to-left (e.g., Arabic)
    TTB,            ///< Top-to-bottom (e.g., Mongolian)
    BTT             ///< Bottom-to-top
};

enum class Script {
    /// Common script (inherited)
    Common,
    Inherited,

    /// Latin scripts
    Latin,
    Greek,
    Cyrillic,
    Armenian,
    Hebrew,
    Arabic,

    /// Indic scripts
    Devanagari,
    Bengali,
    Gurmukhi,
    Gujarati,
    Oriya,
    Tamil,
    Telugu,
    Kannada,
    Malayalam,
    Sinhala,
    Thai,
    Lao,
    Tibetan,

    /// East Asian scripts
    Han,
    Hiragana,
    Katakana,
    Hangul,

    /// Southeast Asian scripts
    Khmer,
    Myanmar,

    /// Other scripts
    Georgian,
    Ethiopic,
    Cherokee,

    /// Symbols and notations
    Mathematical,
    Currency,
    Unknown
};

/// Detect script from Unicode code point
[[nodiscard]] Script detect_script(CodePoint cp);

// ============================================================================
// Hit Testing
// ============================================================================

struct TextHitTestResult {
    i32 character_index{0};     ///< Index in the original text string
    Vec2 position{0, 0};        ///< Position of the character
    Rect bounding_box{0, 0, 0, 0}; ///< Bounding box of the character
    bool is_trailing{false};    ///< True if hit is on trailing edge
};

// ============================================================================
// Text Selection
// ============================================================================

struct TextSelection {
    usize start_index{0};
    usize end_index{0};

    [[nodiscard]] bool is_valid() const {
        return start_index < end_index;
    }

    [[nodiscard]] bool is_empty() const {
        return start_index == end_index;
    }

    [[nodiscard]] usize length() const {
        return end_index - start_index;
    }
};

// ============================================================================
// Font Fallback
// ============================================================================

struct FontFallbackResult {
    String font_family;
    usize char_count;  ///< Number of characters this font can render
};

} // namespace lithium::beryl
