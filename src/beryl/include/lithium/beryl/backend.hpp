#pragma once

#include "lithium/beryl/types.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <vector>

namespace lithium::beryl {

// Forward declarations
class Font;
class GlyphCache;

// ============================================================================
// Rasterization Backend Type
// ============================================================================

enum class FontBackendType {
    Auto,           ///< Auto-detect best backend
    DirectWrite,    ///< DirectWrite (Windows)
    FreeType,       ///< FreeType (Linux/Android)
    CoreText,       ///< Core Text (macOS/iOS)
};

/// Get human-readable name for backend type
[[nodiscard]] inline const char* font_backend_name(FontBackendType type) {
    switch (type) {
        case FontBackendType::Auto: return "Auto";
        case FontBackendType::DirectWrite: return "DirectWrite";
        case FontBackendType::FreeType: return "FreeType";
        case FontBackendType::CoreText: return "CoreText";
        default: return "Unknown";
    }
}

/// Get the best available font backend for the current platform
[[nodiscard]] FontBackendType get_preferred_font_backend();

// ============================================================================
// Font Backend Capabilities
// ============================================================================

struct FontBackendCapabilities {
    bool supports_subpixel_positioning{false};
    bool supports_color_fonts{false};
    bool supports_variable_fonts{false};
    bool supports_ligatures{true};
    bool supports_opentype_features{true};
    bool supports_glyph_outline_extraction{true};
    i32 max_glyph_texture_size{4096};

    /// Supported antialiasing modes
    std::vector<TextAntialiasing> antialiasing_modes;

    /// Support for specific OpenType features
    std::vector<String> supported_features;
};

// ============================================================================
// Glyph Bitmap
// ============================================================================

struct GlyphBitmap {
    std::vector<u8> pixels;  ///< Pixel data
    i32 width{0};            ///< Bitmap width
    i32 height{0};           ///< Bitmap height
    i32 stride{0};           ///< Bytes per row
    i32 bearing_x{0};        ///< Horizontal bearing from origin
    i32 bearing_y{0};        ///< Vertical bearing from baseline
    f32 advance_x{0};        ///< Horizontal advance
    f32 advance_y{0};        ///< Vertical advance (usually 0)

    /// Check if bitmap is valid
    [[nodiscard]] bool is_valid() const {
        return !pixels.empty() && width > 0 && height > 0;
    }

    /// Get pixel at position (no bounds checking)
    [[nodiscard]] u8 get_pixel(i32 x, i32 y) const {
        return pixels[y * stride + x];
    }
};

// ============================================================================
// Glyph Outline
// ============================================================================

enum class OutlineCommand {
    MoveTo,
    LineTo,
    QuadTo,
    CubeTo,
    Close
};

struct OutlinePoint {
    f32 x, y;
};

struct OutlineSegment {
    OutlineCommand command;
    std::vector<OutlinePoint> points;
};

struct GlyphOutline {
    std::vector<OutlineSegment> segments;
    Rect bounds{0, 0, 0, 0};

    /// Convert to path string for debugging
    [[nodiscard]] String to_string() const;
};

// ============================================================================
// Font Backend Interface
// ============================================================================

/// Abstract font rasterization backend interface
/// All font backends (DirectWrite, FreeType, CoreText) must implement this
class IFontBackend {
public:
    virtual ~IFontBackend() = default;

    /// Get backend type
    [[nodiscard]] virtual FontBackendType type() const noexcept = 0;

    /// Get backend capabilities
    [[nodiscard]] virtual const FontBackendCapabilities& capabilities() const noexcept = 0;

    /// Load a font from file
    [[nodiscard]] virtual std::unique_ptr<Font> load_font(
        const String& path,
        f32 size) = 0;

    /// Load a font from memory
    [[nodiscard]] virtual std::unique_ptr<Font> load_font_from_memory(
        const void* data,
        usize size,
        f32 font_size) = 0;

    /// Get a system font by description
    [[nodiscard]] virtual std::unique_ptr<Font> get_system_font(
        const FontDescription& desc) = 0;

    /// Create a glyph cache for efficient rendering
    [[nodiscard]] virtual std::unique_ptr<GlyphCache> create_glyph_cache(
        Font& font,
        i32 texture_size = 2048) = 0;

    /// Set text rendering mode
    virtual void set_rendering_mode(TextRenderingMode mode) = 0;

    /// Set antialiasing mode
    virtual void set_antialiasing(TextAntialiasing mode) = 0;

protected:
    IFontBackend() = default;
};

// ============================================================================
// Font Backend Factory
// ============================================================================

/// Factory function to create font backends
using FontBackendFactory = std::function<std::unique_ptr<IFontBackend>()>;

/// Register a font backend factory for a specific type
void register_font_backend_factory(FontBackendType type, FontBackendFactory factory);

/// Create a font backend instance
[[nodiscard]] std::unique_ptr<IFontBackend> create_font_backend(FontBackendType type);

/// Initialize the default font backend for the current platform
[[nodiscard]] std::unique_ptr<IFontBackend> initialize_default_font_backend();

} // namespace lithium::beryl
