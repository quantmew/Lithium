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

// ShapedText methods
f32 ShapedText::get_x_for_index(usize index) const {
    for (const auto& glyph : glyphs) {
        if (glyph.cluster == index) {
            return glyph.x_offset;
        }
    }
    return total_width;
}

usize ShapedText::get_index_for_x(f32 x) const {
    for (const auto& glyph : glyphs) {
        if (x < glyph.x_offset + glyph.x_advance) {
            return glyph.cluster;
        }
    }
    return glyphs.empty() ? 0 : glyphs.back().cluster;
}

// TextShaper implementation
TextShaper::TextShaper() = default;
TextShaper::~TextShaper() = default;

ShapedText TextShaper::shape(const String& text, Font& font, TextDirection direction) {
    if (direction == TextDirection::LeftToRight) {
        return simple_shape(text, font);
    } else {
        // For RTL, we still use simple shaping but reverse the result
        auto result = simple_shape(text, font);
        std::reverse(result.glyphs.begin(), result.glyphs.end());
        return result;
    }
}

ShapedText TextShaper::shape_with_fallback(
    const String& text,
    Font& primary_font,
    const std::vector<Font*>& fallback_fonts,
    TextDirection direction)
{
    // Simple implementation - only use primary font
    return shape(text, primary_font, direction);
}

ShapedText TextShaper::simple_shape(const String& text, Font& font) {
    ShapedText result;
    result.total_width = 0;
    result.total_height = font.metrics().line_height();
    result.font = &font;

    auto metrics = font.metrics();

    // Simple left-to-right shaping
    usize cluster = 0;
    for (usize i = 0; i < text.size(); ) {
        // Decode UTF-8 code point
        auto decode = unicode::utf8_decode(text.data() + i, text.size() - i);
        unicode::CodePoint cp = decode.code_point;
        i += decode.bytes_consumed;

        auto glyph = font.get_glyph(cp);
        if (!glyph) continue;

        ShapedGlyph shaped;
        shaped.glyph_id = glyph->id;
        shaped.x_offset = result.total_width;
        shaped.y_offset = 0;
        shaped.x_advance = glyph->advance_width;
        shaped.y_advance = 0;
        shaped.cluster = cluster;

        result.glyphs.push_back(shaped);
        result.total_width += glyph->advance_width;

        cluster++;
    }

    // Apply kerning if available
    apply_kerning(result.glyphs, font);

    // Add letter spacing
    if (m_letter_spacing != 0) {
        for (auto& glyph : result.glyphs) {
            glyph.x_advance += m_letter_spacing;
            result.total_width += m_letter_spacing;
        }
    }

    return result;
}

void TextShaper::apply_kerning(std::vector<ShapedGlyph>& glyphs, Font& font) {
    // Placeholder for kerning implementation
    // Real implementation would query font kerning tables
    (void)glyphs;
    (void)font;
}

// ============================================================================
// Script Detection
// ============================================================================

namespace script {

Script detect_script(unicode::CodePoint cp) {
    // Simple script detection based on Unicode ranges
    if (cp < 0x80) return Script::Latin;

    if (cp >= 0x0400 && cp < 0x0500) return Script::Cyrillic;
    if (cp >= 0x0370 && cp < 0x0400) return Script::Greek;
    if (cp >= 0x0600 && cp < 0x0700) return Script::Arabic;
    if (cp >= 0x0590 && cp < 0x0600) return Script::Hebrew;
    if (cp >= 0x4E00 && cp < 0xA000) return Script::Han;
    if (cp >= 0x3040 && cp < 0x3100) return Script::Hiragana;
    if (cp >= 0x30A0 && cp < 0x3100) return Script::Katakana;
    if (cp >= 0xAC00 && cp < 0xD7A0) return Script::Hangul;
    if (cp >= 0x0E00 && cp < 0x0E80) return Script::Thai;
    if (cp >= 0x0900 && cp < 0x0980) return Script::Devanagari;

    return Script::Common;
}

Script detect_dominant_script(const String& text) {
    if (text.empty()) return Script::Unknown;

    std::unordered_map<Script, int> counts;
    Script dominant = Script::Unknown;
    int max_count = 0;

    for (auto it = text.code_points_begin(); it != text.code_points_end(); ++it) {
        unicode::CodePoint cp = *it;
        Script script = detect_script(cp);
        counts[script]++;
        if (counts[script] > max_count) {
            max_count = counts[script];
            dominant = script;
        }
    }

    return dominant;
}

bool is_rtl_script(Script script) {
    return script == Script::Arabic || script == Script::Hebrew;
}

} // namespace script

} // namespace lithium::text
