#pragma once

#include "lithium/beryl/types.hpp"
#include "lithium/beryl/backend.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace lithium::beryl {

// Forward declarations
class IFontBackend;

// ============================================================================
// Font
// ============================================================================

/// Represents a loaded font at a specific size
class Font {
public:
    virtual ~Font() = default;

    /// Get font description
    [[nodiscard]] virtual const FontDescription& description() const noexcept = 0;

    /// Get font metrics
    [[nodiscard]] virtual const FontMetrics& metrics() const noexcept = 0;

    /// Get glyph metrics for a code point
    [[nodiscard]] virtual GlyphMetrics get_glyph_metrics(CodePoint cp) = 0;

    /// Rasterize a glyph to a bitmap
    [[nodiscard]] virtual GlyphBitmap rasterize_glyph(CodePoint cp) = 0;

    /// Extract glyph outline (for scalable rendering)
    [[nodiscard]] virtual GlyphOutline get_glyph_outline(CodePoint cp) = 0;

    /// Get kerning between two glyphs
    [[nodiscard]] virtual f32 get_kerning(CodePoint left, CodePoint right) = 0;

    /// Check if font supports a code point
    [[nodiscard]] virtual bool has_glyph(CodePoint cp) const = 0;

    /// Get all supported code points (optional, can be expensive)
    [[nodiscard]] virtual std::vector<CodePoint> get_supported_codepoints() const = 0;

    /// Measure text width
    [[nodiscard]] virtual f32 measure_text(const String& text) = 0;

    /// Measure single character
    [[nodiscard]] virtual f32 measure_char(CodePoint cp) = 0;

    /// Get underlying backend (for advanced use cases)
    [[nodiscard]] virtual IFontBackend* backend() noexcept = 0;

protected:
    Font() = default;
};

// ============================================================================
// Font Family
// ============================================================================

/// Represents a font family (e.g., "Arial", "Segoe UI")
class FontFamily {
public:
    virtual ~FontFamily() = default;

    /// Get family name
    [[nodiscard]] virtual String name() const = 0;

    /// Check if family is available on system
    [[nodiscard]] virtual bool is_available() const = 0;

    /// Get all available styles in this family
    [[nodiscard]] virtual std::vector<FontDescription> available_styles() const = 0;

    /// Create a font instance with specific properties
    [[nodiscard]] virtual std::unique_ptr<Font> create_font(
        f32 size,
        FontWeight weight = FontWeight::Normal,
        FontStyle style = FontStyle::Normal,
        FontStretch stretch = FontStretch::Normal) = 0;

protected:
    FontFamily() = default;
};

// ============================================================================
// Font Manager
// ============================================================================

/// Manages font loading and caching
class FontManager {
public:
    virtual ~FontManager() = default;

    /// Register a font from file
    virtual bool register_font(
        const String& family_name,
        const String& file_path) = 0;

    /// Register a font from memory
    virtual bool register_font_from_memory(
        const String& family_name,
        const void* data,
        usize size) = 0;

    /// Get a font family by name
    [[nodiscard]] virtual std::shared_ptr<FontFamily> get_family(
        const String& name) = 0;

    /// Get all available font families
    [[nodiscard]] virtual std::vector<String> available_families() const = 0;

    /// Create a font by description (with fallback)
    [[nodiscard]] virtual std::unique_ptr<Font> create_font(
        const FontDescription& desc) = 0;

    /// Find best matching font for given characters
    [[nodiscard]] virtual std::unique_ptr<Font> find_font_for_text(
        const String& text,
        const FontDescription& base_desc) = 0;

    /// Set fallback font chain
    virtual void set_fallback_fonts(const std::vector<String>& families) = 0;

    /// Get current fallback font chain
    [[nodiscard]] virtual std::vector<String> fallback_fonts() const = 0;

protected:
    FontManager() = default;
};

// ============================================================================
// Font Manager Factory
// ============================================================================

/// Create a font manager for the current platform
[[nodiscard]] std::unique_ptr<FontManager> create_font_manager(IFontBackend& backend);

} // namespace lithium::beryl
