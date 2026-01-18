/**
 * Text Rendering Architecture
 *
 * This file defines the architecture for high-performance text rendering
 * with multiple backend support (OpenGL, Direct2D, Software)
 */

#pragma once

#include "lithium/core/types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace lithium::platform {

// ============================================================================
// Font Metrics
// ============================================================================

/**
 * @brief Font metrics (system-independent)
 */
struct FontMetrics {
    f32 ascent;          // Distance from baseline to top of em square
    f32 descent;         // Distance from baseline to bottom of em square
    f32 line_height;     // Total line height
    f32 cap_height;       // Height of uppercase letters
    f32 x_height;         // Height of lowercase letter 'x'
    f32 whitespace_width;  // Width of space character
    f32 avg_char_width;    // Average character width
};

/**
 * @brief Font family categories
 */
enum class FontFamily {
    SansSerif = 0,
    Serif = 1,
    Monospace = 2,
    Cursive = 3,
    Fantasy = 4
};

/**
 * @brief Font weight classifications
 */
enum class FontWeight {
    thin = 100,
    extralight = 200,
    light = 300,
    normal = 400,
    medium = 500,
    semibold = 600,
    bold = 700,
    extrabold = 800
};

// ============================================================================
// Font Manager Interface
// ============================================================================

/**
 * @brief Abstract font manager interface
 *
 * Loads fonts, provides metrics, and manages font caching
 */
class FontManager {
public:
    virtual ~FontManager() = default;

    /**
     * @brief Get the default font family and size
     */
    [[nodiscard]] static std::unique_ptr<FontManager> create();

    /**
     * @brief Load a font from file
     */
    [[nodiscard]] virtual bool load_font_from_file(const String& font_path) = 0;

    /**
     * @brief Get the default system font
     */
    [[nodiscard]] virtual const FontMetrics& get_default_metrics() const = 0;

    /**
     * @brief Get metrics for a specific font family
     */
    [[nodiscard]] virtual const FontMetrics& get_metrics(
        const String& font_family,
        f32 size,
        FontWeight weight
    ) = 0;

    /**
     * @brief Rasterize a character to a bitmap
     */
    [[nodiscard]] virtual Bitmap rasterize_char(
        char32_t codepoint,
        const String& font_family,
        f32 size,
        FontWeight weight,
        const Color& color
    ) = 0;
};

// ============================================================================
// Glyph Cache
// ============================================================================

/**
 * @brief Cached glyph information
 */
struct GlyphInfo {
    char32_t codepoint;
    std::size_t hash;     // Hash for caching

    // Rendering information
    u8* bitmap;         // Rasterized glyph bitmap
    f32 bitmap_width;
    f32 bitmap_height;
    f32 bitmap_left;
    f32 bitmap_top;

    // Glyph metrics
    f32 advance_x;       // Distance to advance cursor after this glyph

    // Cleanup
    void destroy() {
        if (bitmap) {
            delete[] bitmap;
            bitmap = nullptr;
        }
    }
};

/**
 * @brief Text rendering engine interface
 */
class TextRenderer {
public:
    virtual ~TextRenderer() = default;

    /**
     * @brief Create a platform-specific text renderer
     */
    [[nodiscard]] static std::unique_ptr<TextRenderer> create();

    /**
     * @brief Render text to the graphics context
     */
    [[nodiscard]] virtual bool render_text(
        GraphicsContext& graphics,
        const PointF& position,
        const String& text,
        const String& font_family,
        f32 size,
        FontWeight weight,
        const Color& color
    ) = 0;

    /**
     * @brief Measure text width
     */
    [[nodiscard]] virtual f32 measure_text(
        const String& text,
        const String& font_family,
        f32 size,
        FontWeight weight
    ) = 0;
};

// ============================================================================
// Platform-Specific Implementations
// ============================================================================

// Forward declarations for platform implementations
#ifdef _WIN32

/**
 * Windows GDI Text Renderer
 */
class GdiTextRenderer : public TextRenderer {
public:
    ~GdiTextRenderer() override;

    [[nodiscard]] static std::unique_ptr<TextRenderer> create();

    bool render_text(
        GraphicsContext& graphics,
        const PointF& position,
        const String& text,
        const String& font_family,
        f32 size,
        FontWeight weight,
        const Color& color
    ) override;

    f32 measure_text(
        const String& text,
        const String& font_family,
        f32 size,
        FontWeight weight
    ) override;

private:
    // GDI objects
    void* hdc{nullptr};  // Device context
    void* hfont{nullptr};  // Font handle

    // Cached GDI objects
    std::unordered_map<std::string, void*> m_fonts;
};

#elif __linux__

/**
 * Linux FreeType Text Renderer
 */
class FreeTypeTextRenderer : public TextRenderer {
public:
    [[nodiscard]] static std::unique_ptr<TextRenderer> create();

    // ... implementation ...
};

#elif __APPLE__

/**
 * macOS CoreText Text Renderer
 */
class CoreTextTextRenderer : public TextRenderer {
public:
    [[nodiscard]] static std::unique_ptr<TextRenderer> create();

    // ... implementation ...
#endif

// ============================================================================
// Shared Font Manager
// ============================================================================

#ifdef LITHIUM_HARDWARE_ACCELERATED

/**
 * Font manager that uses FreeType (Linux) or platform APIs (Windows/macOS)
 */
class PlatformFontManager : public FontManager {
public:
    ~PlatformFontManager() override;

    [[nodiscard]] static std::unique_ptr<FontManager> create() {
        #ifdef _WIN32
            return std::make_unique<GdiFontManager>();
        #elif __linux__
            return std::make_unique<FreeTypeFontManager>();
        #elif __APPLE__
            return std::make_unique<CoreTextFontManager>();
        #else
            return std::make_unique<PlatformFontManager>();
        #endif
    }

    bool load_font_from_file(const String& font_path) override {
        // TODO: Load font file using platform APIs
        (void)font_path;
        return false;
    }

    const FontMetrics& get_default_metrics() const override {
        // Get system default font metrics
        #ifdef _WIN32
        // Query Windows system font settings
        LOG(INFO, "Using Windows system fonts");
        #elif __linux__
        // Query fontconfig
        LOG(INFO, "Using Linux fontconfig fonts");
        #elif __APPLE__
        // Query CoreText
        LOG(INFO, "Using macOS CoreText fonts");
        #endif

        static FontMetrics metrics = get_system_metrics();
        return metrics;
    }

private:
    // Query system metrics using platform APIs
    [[nodiscard]] static FontMetrics get_system_metrics() {
        // Platform-specific implementation
        #ifdef _WIN32
        return {13.0f, 0.0f, 16.0f, 15.0f, 10.0f, 5.0f, 7.0f};
        #elif __linux__
        return {13.0f, 0.0f, 16.0f, 15.0f, 10.0f, 5.0f, 7.0f};
        #elif __APPLE__
        return {13.0f, 0.0f, 16.0f, 15.0f, 10.0f, 5.0f, 7.0f};
        #else
        return {13.0f, 0.0f, 16.0f, 15.0f, 10.0f, 5.0f, 7.0f};
        #endif
    }

    const FontMetrics& get_metrics(
        const String& font_family,
        f32 size,
        FontWeight weight
    ) override {
        // Query font metrics
        // TODO: Return actual metrics from loaded fonts
        return get_system_metrics();
    }

    Bitmap rasterize_char(
        char32_t codepoint,
        const String& font_family,
        f32 size,
        FontWeight weight,
        const Color& color
    ) override {
        // TODO: Use platform APIs to render glyph
        (void)codepoint;
        (void)font_family;
        (void)size;
        (void)weight;
        (void)color;

        // Create a test bitmap
        auto width = static_cast<i32>(size * 2);  // Scale up for better quality
        auto height = static_cast<i32>(size * 2);

        auto bitmap = Bitmap(width, height, 4);
        bitmap.fill({255, 255, 255, 255});  // Red to verify visibility
        return bitmap;
    }
};

} // namespace lithium::platform
