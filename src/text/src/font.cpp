/**
 * Font implementation (stub with FreeType integration)
 */

#include "lithium/text/font.hpp"
#include <cmath>

// FreeType headers (optional - compile without if not available)
#ifdef LITHIUM_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace lithium::text {

// ============================================================================
// FontDescription
// ============================================================================

bool FontDescription::operator==(const FontDescription& other) const {
    return family == other.family &&
           std::abs(size - other.size) < 0.01f &&
           bold == other.bold &&
           italic == other.italic;
}

std::size_t FontDescriptionHash::operator()(const FontDescription& desc) const {
    std::size_t h = std::hash<std::string>{}(std::string(desc.family.data(), desc.family.size()));
    h ^= std::hash<float>{}(desc.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(desc.bold) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(desc.italic) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// ============================================================================
// Stub Font implementation
// ============================================================================

class StubFont : public Font {
public:
    StubFont(const String& family, f32 size, bool bold, bool italic)
        : m_family(family)
        , m_size(size)
        , m_bold(bold)
        , m_italic(italic)
    {
        // Initialize with reasonable default metrics
        m_metrics.ascender = size * 0.8f;
        m_metrics.descender = -size * 0.2f;
        m_metrics.line_gap = size * 0.1f;
        m_metrics.units_per_em = 1000;
        m_metrics.x_height = size * 0.5f;
        m_metrics.cap_height = size * 0.7f;
    }

    const String& family() const override { return m_family; }
    f32 size() const override { return m_size; }
    bool is_bold() const override { return m_bold; }
    bool is_italic() const override { return m_italic; }

    FontMetrics metrics() const override { return m_metrics; }

    std::optional<Glyph> get_glyph(unicode::CodePoint cp) const override {
        // Return a basic glyph for any character
        Glyph g;
        g.id = cp;
        g.advance_width = m_size * 0.6f;
        g.left_side_bearing = 0;
        g.x_min = 0;
        g.y_min = 0;
        g.x_max = g.advance_width;
        g.y_max = m_size;
        return g;
    }

    std::optional<GlyphBitmap> rasterize_glyph(unicode::CodePoint cp) const override {
        // Return empty bitmap - proper implementation would render glyph
        GlyphBitmap bitmap;
        bitmap.width = static_cast<i32>(m_size * 0.6f);
        bitmap.height = static_cast<i32>(m_size);
        bitmap.bearing_x = 0;
        bitmap.bearing_y = static_cast<i32>(m_size * 0.8f);
        bitmap.advance = m_size * 0.6f;
        bitmap.pixels.resize(bitmap.width * bitmap.height, 0);
        return bitmap;
    }

    f32 get_kerning(unicode::CodePoint left, unicode::CodePoint right) const override {
        // No kerning in stub
        return 0;
    }

    f32 measure_text(const String& text) const override {
        // Simple calculation - each character is same width
        f32 width = 0;
        for (usize i = 0; i < text.size(); ++i) {
            width += m_size * 0.6f;
        }
        return width;
    }

    f32 measure_char(unicode::CodePoint cp) const override {
        return m_size * 0.6f;
    }

private:
    String m_family;
    f32 m_size;
    bool m_bold;
    bool m_italic;
    FontMetrics m_metrics;
};

// ============================================================================
// FreeType Font implementation (if available)
// ============================================================================

#ifdef LITHIUM_HAS_FREETYPE

class FreeTypeFont : public Font {
public:
    FreeTypeFont(FT_Library library, const String& path, f32 size, bool bold, bool italic)
        : m_library(library)
        , m_size(size)
        , m_bold(bold)
        , m_italic(italic)
    {
        FT_Error error = FT_New_Face(library, path.data(), 0, &m_face);
        if (error) {
            return;
        }

        FT_Set_Pixel_Sizes(m_face, 0, static_cast<FT_UInt>(size));

        m_family = String(m_face->family_name);

        // Extract metrics
        m_metrics.ascender = m_face->size->metrics.ascender / 64.0f;
        m_metrics.descender = m_face->size->metrics.descender / 64.0f;
        m_metrics.line_gap = (m_face->size->metrics.height / 64.0f) -
                            (m_metrics.ascender - m_metrics.descender);
        m_metrics.units_per_em = static_cast<f32>(m_face->units_per_EM);
    }

    ~FreeTypeFont() override {
        if (m_face) {
            FT_Done_Face(m_face);
        }
    }

    const String& family() const override { return m_family; }
    f32 size() const override { return m_size; }
    bool is_bold() const override { return m_bold; }
    bool is_italic() const override { return m_italic; }

    FontMetrics metrics() const override { return m_metrics; }

    std::optional<Glyph> get_glyph(unicode::CodePoint cp) const override {
        if (!m_face) return std::nullopt;

        FT_UInt glyph_index = FT_Get_Char_Index(m_face, cp);
        if (glyph_index == 0) return std::nullopt;

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT)) {
            return std::nullopt;
        }

        Glyph g;
        g.id = glyph_index;
        g.advance_width = m_face->glyph->advance.x / 64.0f;
        g.left_side_bearing = m_face->glyph->metrics.horiBearingX / 64.0f;
        g.x_min = m_face->glyph->metrics.horiBearingX / 64.0f;
        g.y_min = (m_face->glyph->metrics.horiBearingY - m_face->glyph->metrics.height) / 64.0f;
        g.x_max = g.x_min + m_face->glyph->metrics.width / 64.0f;
        g.y_max = m_face->glyph->metrics.horiBearingY / 64.0f;
        return g;
    }

    std::optional<GlyphBitmap> rasterize_glyph(unicode::CodePoint cp) const override {
        if (!m_face) return std::nullopt;

        FT_UInt glyph_index = FT_Get_Char_Index(m_face, cp);
        if (glyph_index == 0) return std::nullopt;

        if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT)) {
            return std::nullopt;
        }

        if (FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL)) {
            return std::nullopt;
        }

        FT_Bitmap& bitmap = m_face->glyph->bitmap;

        GlyphBitmap result;
        result.width = bitmap.width;
        result.height = bitmap.rows;
        result.bearing_x = m_face->glyph->bitmap_left;
        result.bearing_y = m_face->glyph->bitmap_top;
        result.advance = m_face->glyph->advance.x / 64.0f;

        result.pixels.resize(result.width * result.height);
        for (i32 y = 0; y < result.height; ++y) {
            for (i32 x = 0; x < result.width; ++x) {
                result.pixels[y * result.width + x] = bitmap.buffer[y * bitmap.pitch + x];
            }
        }

        return result;
    }

    f32 get_kerning(unicode::CodePoint left, unicode::CodePoint right) const override {
        if (!m_face || !FT_HAS_KERNING(m_face)) return 0;

        FT_UInt left_index = FT_Get_Char_Index(m_face, left);
        FT_UInt right_index = FT_Get_Char_Index(m_face, right);

        FT_Vector kerning;
        FT_Get_Kerning(m_face, left_index, right_index, FT_KERNING_DEFAULT, &kerning);
        return kerning.x / 64.0f;
    }

    f32 measure_text(const String& text) const override {
        f32 width = 0;
        unicode::CodePoint prev = 0;

        // Simple iteration assuming ASCII for now
        for (usize i = 0; i < text.size(); ++i) {
            unicode::CodePoint cp = static_cast<unicode::CodePoint>(text[i]);
            if (auto glyph = get_glyph(cp)) {
                width += glyph->advance_width;
                if (prev) {
                    width += get_kerning(prev, cp);
                }
                prev = cp;
            }
        }
        return width;
    }

    f32 measure_char(unicode::CodePoint cp) const override {
        if (auto glyph = get_glyph(cp)) {
            return glyph->advance_width;
        }
        return m_size * 0.6f;
    }

private:
    FT_Library m_library;
    FT_Face m_face{nullptr};
    String m_family;
    f32 m_size;
    bool m_bold;
    bool m_italic;
    FontMetrics m_metrics;
};

#endif // LITHIUM_HAS_FREETYPE

// ============================================================================
// FontContext implementation
// ============================================================================

struct FontContext::FontData {
#ifdef LITHIUM_HAS_FREETYPE
    FT_Library library{nullptr};
#endif
};

FontContext::FontContext() : m_data(std::make_unique<FontData>()) {
#ifdef LITHIUM_HAS_FREETYPE
    FT_Init_FreeType(&m_data->library);
#endif
}

FontContext::~FontContext() {
#ifdef LITHIUM_HAS_FREETYPE
    if (m_data->library) {
        FT_Done_FreeType(m_data->library);
    }
#endif
}

std::shared_ptr<Font> FontContext::load_font(const String& path, f32 size) {
#ifdef LITHIUM_HAS_FREETYPE
    if (m_data->library) {
        return std::make_shared<FreeTypeFont>(m_data->library, path, size, false, false);
    }
#endif
    return std::make_shared<StubFont>("sans-serif"_s, size, false, false);
}

std::shared_ptr<Font> FontContext::get_font(const FontDescription& desc) {
    auto it = m_cache.find(desc);
    if (it != m_cache.end()) {
        return it->second;
    }

    // Try to find registered font
    auto reg_it = m_registered_fonts.find(desc.family);
    if (reg_it != m_registered_fonts.end() && !reg_it->second.empty()) {
        auto font = load_font(reg_it->second[0], desc.size);
        m_cache[desc] = font;
        return font;
    }

    // Create stub font
    auto font = std::make_shared<StubFont>(desc.family, desc.size, desc.bold, desc.italic);
    m_cache[desc] = font;
    return font;
}

void FontContext::register_font(const String& family, const String& path, bool bold, bool italic) {
    m_registered_fonts[family].push_back(path);
}

void FontContext::set_fallback_fonts(const std::vector<String>& families) {
    m_fallback_families = families;
}

void FontContext::clear_cache() {
    m_cache.clear();
}

std::vector<String> FontContext::get_system_font_paths() {
    std::vector<String> paths;
#ifdef __linux__
    paths.push_back("/usr/share/fonts"_s);
    paths.push_back("/usr/local/share/fonts"_s);
    paths.push_back("~/.fonts"_s);
#elif defined(_WIN32)
    paths.push_back("C:\\Windows\\Fonts"_s);
#elif defined(__APPLE__)
    paths.push_back("/Library/Fonts"_s);
    paths.push_back("/System/Library/Fonts"_s);
    paths.push_back("~/Library/Fonts"_s);
#endif
    return paths;
}

// ============================================================================
// Font matching
// ============================================================================

namespace font_matching {

String resolve_generic_family(const String& family) {
    if (family == "serif"_s) {
#ifdef __linux__
        return "DejaVu Serif"_s;
#else
        return "Times New Roman"_s;
#endif
    } else if (family == "sans-serif"_s) {
#ifdef __linux__
        return "DejaVu Sans"_s;
#else
        return "Arial"_s;
#endif
    } else if (family == "monospace"_s) {
#ifdef __linux__
        return "DejaVu Sans Mono"_s;
#else
        return "Courier New"_s;
#endif
    }
    return family;
}

std::shared_ptr<Font> find_best_match(
    FontContext& context,
    const std::vector<String>& families,
    f32 size,
    bool bold,
    bool italic) {

    for (const auto& family : families) {
        String resolved = resolve_generic_family(family);
        FontDescription desc{resolved, size, bold, italic};
        auto font = context.get_font(desc);
        if (font) {
            return font;
        }
    }

    // Fallback to default
    return context.get_font({"sans-serif"_s, size, bold, italic});
}

} // namespace font_matching

} // namespace lithium::text
