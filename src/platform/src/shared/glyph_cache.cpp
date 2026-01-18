/**
 * Glyph Cache Implementation
 *
 * Implements a glyph cache using FreeType for rasterization and
 * texture atlas for storage. Provides LRU eviction when full.
 */

#include "lithium/core/logger.hpp"
#include <unordered_map>
#include <list>
#include <memory>
#include <functional>
#include <algorithm>

namespace lithium::platform {

using namespace lithium;

// ============================================================================
// Glyph Cache Entry
// ============================================================================

/**
 * @brief Single cached glyph entry
 */
struct GlyphCacheEntry {
    char32_t codepoint;           // Unicode codepoint
    String font_family;           // Font family name
    f32 font_size;                // Font size in pixels
    u32 hash;                     // Cache key hash

    // Glyph metrics
    f32 advance_x;                // Horizontal advance
    f32 bitmap_left;              // Left bearing
    f32 bitmap_top;               // Top bearing
    f32 bitmap_width;             // Bitmap width
    f32 bitmap_height;            // Bitmap height

    // Texture coordinates in atlas (normalized 0-1)
    f32 uv_x, uv_y;               // Top-left corner
    f32 uv_width, uv_height;      // Dimensions

    // Cache management
    usize access_count;           // Number of times accessed
    u64 last_access;              // Last access timestamp

    GlyphCacheEntry()
        : codepoint(0)
        , font_size(0)
        , hash(0)
        , advance_x(0)
        , bitmap_left(0)
        , bitmap_top(0)
        , bitmap_width(0)
        , bitmap_height(0)
        , uv_x(0)
        , uv_y(0)
        , uv_width(0)
        , uv_height(0)
        , access_count(0)
        , last_access(0)
    {
    }
};

// ============================================================================
// Glyph Atlas (Texture Atlas for Glyph Storage)
// ============================================================================

/**
 * @brief Texture atlas for storing glyph bitmaps
 */
class GlyphAtlas {
public:
    static constexpr usize DEFAULT_WIDTH = 2048;
    static constexpr usize DEFAULT_HEIGHT = 2048;

    GlyphAtlas(usize width = DEFAULT_WIDTH, usize height = DEFAULT_HEIGHT)
        : m_width(width)
        , m_height(height)
        , m_current_y(0)
        , m_current_x(0)
        , m_row_height(0)
    {
        // Allocate atlas data (RGBA8 format)
        m_data = std::make_unique<u8[]>(width * height * 4);
        std::memset(m_data.get(), 0, width * height * 4);

        LITHIUM_LOG_INFO("Created glyph atlas: {}x{}", width, height);
    }

    /**
     * @brief Upload a glyph bitmap to the atlas
     * @return True if uploaded successfully
     */
    [[nodiscard]] bool upload_glyph(
        const u8* bitmap,
        usize bitmap_width,
        usize bitmap_height,
        f32& out_uv_x,
        f32& out_uv_y,
        f32& out_uv_width,
        f32& out_uv_height
    ) {
        // Check if we need to advance to next row
        if (m_current_x + bitmap_width > m_width) {
            m_current_x = 0;
            m_current_y += m_row_height + 1;  // +1 for padding
            m_row_height = 0;
        }

        // Check if we need to advance to next page (not implemented for now)
        if (m_current_y + bitmap_height > m_height) {
            LITHIUM_LOG_ERROR("Glyph atlas full, need to implement multi-page support");
            return false;
        }

        // Update row height
        if (static_cast<usize>(bitmap_height) > m_row_height) {
            m_row_height = bitmap_height;
        }

        // Copy bitmap to atlas (convert grayscale to RGBA)
        for (usize y = 0; y < bitmap_height; ++y) {
            for (usize x = 0; x < bitmap_width; ++x) {
                usize atlas_idx = ((m_current_y + y) * m_width + (m_current_x + x)) * 4;
                u8 pixel_value = bitmap ? bitmap[y * bitmap_width + x] : 255;

                m_data[atlas_idx + 0] = pixel_value;  // R
                m_data[atlas_idx + 1] = pixel_value;  // G
                m_data[atlas_idx + 2] = pixel_value;  // B
                m_data[atlas_idx + 3] = pixel_value;  // A
            }
        }

        // Return UV coordinates (normalized 0-1)
        out_uv_x = static_cast<f32>(m_current_x) / static_cast<f32>(m_width);
        out_uv_y = static_cast<f32>(m_current_y) / static_cast<f32>(m_height);
        out_uv_width = static_cast<f32>(bitmap_width) / static_cast<f32>(m_width);
        out_uv_height = static_cast<f32>(bitmap_height) / static_cast<f32>(m_height);

        // Advance cursor
        m_current_x += bitmap_width + 1;  // +1 for padding

        return true;
    }

    [[nodiscard]] const u8* data() const { return m_data.get(); }
    [[nodiscard]] usize width() const { return m_width; }
    [[nodiscard]] usize height() const { return m_height; }

private:
    std::unique_ptr<u8[]> m_data;
    usize m_width;
    usize m_height;

    // Cursor position for uploading
    usize m_current_x;
    usize m_current_y;
    usize m_row_height;
};

// ============================================================================
// Glyph Cache Implementation
// ============================================================================

/**
 * @brief LRU glyph cache with automatic eviction
 */
class GlyphCacheImpl {
public:
    static constexpr usize MAX_CACHED_GLYPHS = 4096;
    static constexpr f32 EVICTION_RATIO = 0.2f;  // Evict 20% when full

    GlyphCacheImpl()
        : m_access_counter(0)
    {
        m_atlas = std::make_unique<GlyphAtlas>();
        LITHIUM_LOG_INFO("Glyph cache initialized");
    }

    ~GlyphCacheImpl() = default;

    /**
     * @brief Get or cache a glyph
     */
    [[nodiscard]] const GlyphCacheEntry* get_glyph(
        char32_t codepoint,
        const String& font_family,
        f32 font_size
    ) {
        // Generate cache key
        u32 hash = compute_hash(codepoint, font_family, font_size);

        // Check cache
        auto it = m_cache.find(hash);
        if (it != m_cache.end()) {
            // Update access info
            it->second.access_count++;
            it->second.last_access = ++m_access_counter;
            return &it->second;
        }

        // Glyph not cached, need to create it
        // For now, return nullptr (will be implemented with platform integration)
        LITHIUM_LOG_DEBUG("Glyph not cached: U+{:X} ({})", static_cast<u32>(codepoint), font_family);

        // Check if cache is full, evict if necessary
        if (m_cache.size() >= MAX_CACHED_GLYPHS) {
            evict_lru(static_cast<usize>(MAX_CACHED_GLYPHS * EVICTION_RATIO));
        }

        // Create new cache entry
        auto result = m_cache.emplace(hash, GlyphCacheEntry());
        if (result.second) {
            auto& entry = result.first->second;
            entry.codepoint = codepoint;
            entry.font_family = font_family;
            entry.font_size = font_size;
            entry.hash = hash;
            entry.access_count = 1;
            entry.last_access = ++m_access_counter;

            // TODO: Rasterize glyph using platform APIs and upload to atlas
            // This will be implemented when integrating with DirectWrite/FreeType

            return &entry;
        }

        return nullptr;
    }

    /**
     * @brief Clear the entire cache
     */
    void clear() {
        m_cache.clear();
        m_atlas = std::make_unique<GlyphAtlas>();
        LITHIUM_LOG_INFO("Glyph cache cleared");
    }

    /**
     * @brief Get cache statistics
     */
    void get_stats(usize& size, usize& max_size) const {
        size = m_cache.size();
        max_size = MAX_CACHED_GLYPHS;
    }

    [[nodiscard]] GlyphAtlas* atlas() { return m_atlas.get(); }

private:
    /**
     * @brief Compute hash for cache key
     */
    [[nodiscard]] static u32 compute_hash(
        char32_t codepoint,
        const String& font_family,
        f32 font_size
    ) {
        u32 h = static_cast<u32>(codepoint);
        h = h * 31 + static_cast<u32>(std::hash<String>{}(font_family));
        h = h * 31 + static_cast<u32>(font_size * 10.0f);
        return h;
    }

    /**
     * @brief Evict least recently used glyphs
     */
    void evict_lru(usize count) {
        // Find LRU entries
        std::vector<u32> lru_hashes;
        lru_hashes.reserve(count);

        // Sort by last access time
        std::vector<std::pair<u64, u32>> entries;
        for (const auto& pair : m_cache) {
            entries.emplace_back(pair.second.last_access, pair.first);
        }

        std::sort(entries.begin(), entries.end());

        // Evict oldest entries
        usize evicted = 0;
        for (const auto& [access_time, hash] : entries) {
            if (evicted >= count) break;

            m_cache.erase(hash);
            evicted++;
        }

        LITHIUM_LOG_INFO("Evicted {} glyphs from cache", evicted);
    }

private:
    std::unordered_map<u32, GlyphCacheEntry> m_cache;
    std::unique_ptr<GlyphAtlas> m_atlas;
    u64 m_access_counter;
};

} // namespace lithium::platform
