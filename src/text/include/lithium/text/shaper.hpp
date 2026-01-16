#pragma once

#include "font.hpp"
#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <vector>

namespace lithium::text {

// ============================================================================
// Shaped Glyph
// ============================================================================

struct ShapedGlyph {
    u32 glyph_id{0};
    f32 x_offset{0};      // Offset from origin
    f32 y_offset{0};
    f32 x_advance{0};     // How much to advance after this glyph
    f32 y_advance{0};
    usize cluster{0};     // Index into original text (for cursor positioning)
};

// ============================================================================
// Shaped Text - Result of text shaping
// ============================================================================

struct ShapedText {
    std::vector<ShapedGlyph> glyphs;
    f32 total_width{0};
    f32 total_height{0};
    Font* font{nullptr};

    // Get x position for character at given index
    [[nodiscard]] f32 get_x_for_index(usize index) const;

    // Get character index at given x position
    [[nodiscard]] usize get_index_for_x(f32 x) const;
};

// ============================================================================
// Text Direction
// ============================================================================

enum class TextDirection {
    LeftToRight,
    RightToLeft,
};

// ============================================================================
// Text Shaper - Converts text to positioned glyphs
// ============================================================================

class TextShaper {
public:
    TextShaper();
    ~TextShaper();

    // Shape text with a single font
    [[nodiscard]] ShapedText shape(const String& text, Font& font, TextDirection direction = TextDirection::LeftToRight);

    // Shape text with font fallback
    [[nodiscard]] ShapedText shape_with_fallback(
        const String& text,
        Font& primary_font,
        const std::vector<Font*>& fallback_fonts,
        TextDirection direction = TextDirection::LeftToRight);

    // Settings
    void set_letter_spacing(f32 spacing) { m_letter_spacing = spacing; }
    void set_word_spacing(f32 spacing) { m_word_spacing = spacing; }

private:
    // Simple shaping (no complex script support)
    ShapedText simple_shape(const String& text, Font& font);

    // Apply kerning
    void apply_kerning(std::vector<ShapedGlyph>& glyphs, Font& font);

    f32 m_letter_spacing{0};
    f32 m_word_spacing{0};
};

// ============================================================================
// Text Run - Segment of text with uniform style
// ============================================================================

struct TextRun {
    String text;
    Font* font;
    TextDirection direction;
    usize start_index;  // Index in original text
};

// ============================================================================
// Script and Language Detection
// ============================================================================

namespace script {

enum class Script {
    Latin,
    Cyrillic,
    Greek,
    Arabic,
    Hebrew,
    Han,
    Hiragana,
    Katakana,
    Hangul,
    Thai,
    Devanagari,
    Common,  // Punctuation, numbers, etc.
    Unknown,
};

// Detect script of a code point
[[nodiscard]] Script detect_script(unicode::CodePoint cp);

// Detect dominant script of text
[[nodiscard]] Script detect_dominant_script(const String& text);

// Check if script is right-to-left
[[nodiscard]] bool is_rtl_script(Script script);

} // namespace script

} // namespace lithium::text
