#pragma once

#include "lithium/beryl/types.hpp"
#include "lithium/beryl/backend.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <vector>

namespace lithium::beryl {

// Forward declaration for Color to avoid circular dependency
namespace lithium {
namespace mica {
struct Color;
}
}

// ============================================================================
// Glyph Cache
// ============================================================================

/// Cached glyph data for efficient rendering
struct CachedGlyph {
    u32 glyph_id{0};
    GlyphMetrics metrics;
    Rect texture_rect{0, 0, 0, 0};  ///< Position in texture atlas
    u32 texture_index{0};             ///< Which texture atlas

    /// Check if glyph is valid
    [[nodiscard]] bool is_valid() const {
        return glyph_id != 0 && texture_rect.width > 0;
    }
};

/// Texture atlas for storing glyph bitmaps
class GlyphAtlas {
public:
    virtual ~GlyphAtlas() = default;

    /// Get texture handle (platform-specific)
    [[nodiscard]] virtual void* texture_handle() const noexcept = 0;

    /// Get texture size
    [[nodiscard]] virtual Size texture_size() const noexcept = 0;

    /// Get number of glyphs in atlas
    [[nodiscard]] virtual usize glyph_count() const noexcept = 0;

    /// Clear all cached glyphs
    virtual void clear() = 0;

protected:
    GlyphAtlas() = default;
};

/// Glyph cache for efficient text rendering
class GlyphCache {
public:
    virtual ~GlyphCache() = default;

    /// Get or rasterize a cached glyph
    [[nodiscard]] virtual const CachedGlyph* get_glyph(CodePoint cp) = 0;

    /// Pre-cache a range of characters (e.g., ASCII)
    virtual void preload_range(u32 start, u32 end) = 0;

    /// Clear cache
    virtual void clear() = 0;

    /// Get cache statistics
    struct Statistics {
        usize cache_hits{0};
        usize cache_misses{0};
        usize total_glyphs{0};
        f32 hit_rate{0.0f};
    };

    [[nodiscard]] virtual Statistics get_statistics() const = 0;

    /// Get associated font
    [[nodiscard]] virtual Font* font() noexcept = 0;

protected:
    GlyphCache() = default;
};

// ============================================================================
// Glyph Renderer
// ============================================================================

/// Renders glyphs using cached glyph data
class GlyphRenderer {
public:
    virtual ~GlyphRenderer() = default;

    /// Render a single glyph at position
    virtual void render_glyph(
        const CachedGlyph& glyph,
        Vec2 position,
        const lithium::mica::Color& color) = 0;

    /// Render a run of glyphs
    virtual void render_glyph_run(
        const std::vector<CachedGlyph>& glyphs,
        const std::vector<Vec2>& positions,
        const lithium::mica::Color& color) = 0;

    /// Set subpixel positioning mode
    virtual void set_subpixel_positioning(bool enabled) = 0;

protected:
    GlyphRenderer() = default;
};

/// Create a glyph cache for a font
[[nodiscard]] std::unique_ptr<GlyphCache> create_glyph_cache(
    Font& font,
    i32 atlas_size = 2048);

} // namespace lithium::beryl
