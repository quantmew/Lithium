#pragma once

#include "lithium/beryl/types.hpp"
#include "lithium/beryl/font.hpp"
#include "lithium/beryl/glyph.hpp"
#include "lithium/beryl/text_layout.hpp"
#include "lithium/beryl/backend.hpp"
#include "lithium/core/memory.hpp"
#include <memory>

namespace lithium::beryl {

// Forward declaration
struct EngineCapabilities;

// ============================================================================
// Text Engine
// ============================================================================

/// Main entry point for Beryl text engine
class TextEngine {
public:
    virtual ~TextEngine() = default;

    /// Initialize the engine
    [[nodiscard]] virtual bool initialize() = 0;

    /// Shutdown the engine
    virtual void shutdown() = 0;

    /// Check if engine is initialized
    [[nodiscard]] virtual bool is_initialized() const noexcept = 0;

    /// Get the font backend
    [[nodiscard]] virtual IFontBackend* backend() noexcept = 0;

    /// Get the font manager
    [[nodiscard]] virtual FontManager* font_manager() noexcept = 0;

    /// Create a layout builder
    [[nodiscard]] virtual std::unique_ptr<TextLayoutBuilder> create_layout_builder() = 0;

    /// Create a glyph cache
    [[nodiscard]] virtual std::unique_ptr<GlyphCache> create_glyph_cache(Font& font) = 0;

    /// Create a glyph renderer
    [[nodiscard]] virtual std::unique_ptr<GlyphRenderer> create_glyph_renderer() = 0;

    /// Get engine capabilities
    [[nodiscard]] virtual const EngineCapabilities& capabilities() const = 0;

    /// Set default font description
    virtual void set_default_font(const FontDescription& desc) = 0;

    /// Get default font description
    [[nodiscard]] virtual const FontDescription& default_font() const = 0;

protected:
    TextEngine() = default;
};

// ============================================================================
// Text Engine Factory
// ============================================================================

/// Create a text engine with the specified backend
[[nodiscard]] std::unique_ptr<TextEngine> create_text_engine(
    FontBackendType type = FontBackendType::Auto);

/// Get the global text engine instance
[[nodiscard]] TextEngine* get_global_engine() noexcept;

/// Set the global text engine instance
void set_global_engine(std::unique_ptr<TextEngine> engine);

} // namespace lithium::beryl
