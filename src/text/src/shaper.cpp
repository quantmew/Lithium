/**
 * Text Shaper implementation
 */

#include "lithium/text/shaper.hpp"
#include "lithium/text/font.hpp"

namespace lithium::text {

// ============================================================================
// Shaper placeholder
// ============================================================================

// TextShaper class would handle complex text shaping:
// - Bidi text (right-to-left languages)
// - Ligature substitution
// - Complex scripts (Arabic, Devanagari, etc.)
// - Unicode normalization
//
// For now, this file provides basic left-to-right shaping

struct ShapedGlyph {
    u32 glyph_id;
    f32 x_offset;
    f32 y_offset;
    f32 advance;
    usize cluster;  // Index into original string
};

struct ShapedText {
    std::vector<ShapedGlyph> glyphs;
    f32 total_width;
    f32 total_height;
};

class TextShaper {
public:
    TextShaper() = default;

    ShapedText shape(const String& text, const Font& font) {
        ShapedText result;
        result.total_width = 0;
        result.total_height = font.metrics().line_height();

        unicode::CodePoint prev = 0;

        // Simple left-to-right shaping
        for (usize i = 0; i < text.size(); ++i) {
            unicode::CodePoint cp = static_cast<unicode::CodePoint>(
                static_cast<unsigned char>(text[i]));

            auto glyph = font.get_glyph(cp);
            if (!glyph) continue;

            ShapedGlyph shaped;
            shaped.glyph_id = glyph->id;
            shaped.x_offset = 0;
            shaped.y_offset = 0;
            shaped.advance = glyph->advance_width;
            shaped.cluster = i;

            // Apply kerning
            if (prev) {
                f32 kern = font.get_kerning(prev, cp);
                shaped.x_offset += kern;
            }

            result.glyphs.push_back(shaped);
            result.total_width += shaped.advance + shaped.x_offset;
            prev = cp;
        }

        return result;
    }

    // Shape with script and direction hints
    ShapedText shape(const String& text, const Font& font,
                     const String& script, bool rtl) {
        // For now, just use basic shaping
        auto result = shape(text, font);

        // If RTL, reverse glyph order
        if (rtl) {
            std::reverse(result.glyphs.begin(), result.glyphs.end());
        }

        return result;
    }
};

} // namespace lithium::text
