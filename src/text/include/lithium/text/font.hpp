#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace lithium::text {

// ============================================================================
// Font Metrics
// ============================================================================

struct FontMetrics {
    f32 ascender{0};       // Distance from baseline to top
    f32 descender{0};      // Distance from baseline to bottom (negative)
    f32 line_gap{0};       // Extra spacing between lines
    f32 units_per_em{0};   // Font units per em
    f32 x_height{0};       // Height of lowercase 'x'
    f32 cap_height{0};     // Height of capital letters

    [[nodiscard]] f32 line_height() const { return ascender - descender + line_gap; }
};

// ============================================================================
// Glyph
// ============================================================================

struct Glyph {
    u32 id{0};
    f32 advance_width{0};
    f32 left_side_bearing{0};

    // Bounding box
    f32 x_min{0};
    f32 y_min{0};
    f32 x_max{0};
    f32 y_max{0};
};

// ============================================================================
// Glyph Bitmap
// ============================================================================

struct GlyphBitmap {
    std::vector<u8> pixels;  // Alpha channel only
    i32 width{0};
    i32 height{0};
    i32 bearing_x{0};        // Offset from origin to left edge
    i32 bearing_y{0};        // Offset from baseline to top edge
    f32 advance{0};          // Advance width
};

// ============================================================================
// Font
// ============================================================================

class Font {
public:
    virtual ~Font() = default;

    // Font properties
    [[nodiscard]] virtual const String& family() const = 0;
    [[nodiscard]] virtual f32 size() const = 0;
    [[nodiscard]] virtual bool is_bold() const = 0;
    [[nodiscard]] virtual bool is_italic() const = 0;

    // Metrics
    [[nodiscard]] virtual FontMetrics metrics() const = 0;

    // Glyph access
    [[nodiscard]] virtual std::optional<Glyph> get_glyph(unicode::CodePoint cp) const = 0;
    [[nodiscard]] virtual std::optional<GlyphBitmap> rasterize_glyph(unicode::CodePoint cp) const = 0;

    // Kerning
    [[nodiscard]] virtual f32 get_kerning(unicode::CodePoint left, unicode::CodePoint right) const = 0;

    // Text measurement
    [[nodiscard]] virtual f32 measure_text(const String& text) const = 0;
    [[nodiscard]] virtual f32 measure_char(unicode::CodePoint cp) const = 0;
};

// ============================================================================
// Font Context - Manages font loading and caching
// ============================================================================

struct FontDescription {
    String family{"sans-serif"};
    f32 size{16.0f};
    bool bold{false};
    bool italic{false};

    [[nodiscard]] bool operator==(const FontDescription& other) const;
};

// Hash for FontDescription
struct FontDescriptionHash {
    std::size_t operator()(const FontDescription& desc) const;
};

class FontContext {
public:
    FontContext();
    ~FontContext();

    // Load a font from file
    [[nodiscard]] std::shared_ptr<Font> load_font(const String& path, f32 size);

    // Get a font matching description
    [[nodiscard]] std::shared_ptr<Font> get_font(const FontDescription& desc);

    // Register a font family from file
    void register_font(const String& family, const String& path, bool bold = false, bool italic = false);

    // Set fallback fonts for missing glyphs
    void set_fallback_fonts(const std::vector<String>& families);

    // Clear font cache
    void clear_cache();

    // System fonts
    static std::vector<String> get_system_font_paths();

private:
    struct FontData;
    std::unique_ptr<FontData> m_data;

    std::unordered_map<FontDescription, std::shared_ptr<Font>, FontDescriptionHash> m_cache;
    std::unordered_map<String, std::vector<String>> m_registered_fonts;  // family -> paths
    std::vector<String> m_fallback_families;
};

// ============================================================================
// Font Matching
// ============================================================================

namespace font_matching {

// Generic font families
[[nodiscard]] String resolve_generic_family(const String& family);

// Find best matching font
[[nodiscard]] std::shared_ptr<Font> find_best_match(
    FontContext& context,
    const std::vector<String>& families,
    f32 size,
    bool bold,
    bool italic);

} // namespace font_matching

} // namespace lithium::text
