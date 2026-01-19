#pragma once

/// Beryl Text Engine
/// High-quality text rendering engine for Lithium Browser
///
/// Named after the beryl mineral (Be3Al2(SiO3)6), which includes gemstones
/// like emerald and aquamarine. This engine provides:
///
/// - Platform-native text rasterization:
///   - Windows: DirectWrite (ClearType)
///   - Linux/Android: FreeType
///   - macOS/iOS: Core Text
///
/// - Unicode and complex text support (Arabic, Thai, Indic scripts)
/// - Font fallback and substitution
/// - Text shaping with HarfBuzz integration
/// - High-quality glyph caching and rendering
///
/// Architecture:
/// ┌─────────────────────────────────────┐
/// │   High-Level Text API               │
/// │   (TextLayout, TextRun, etc.)       │
/// └─────────────────────────────────────┘
///                ↓
/// ┌─────────────────────────────────────┐
/// │   Text Shaping (HarfBuzz)           │
/// │   (Script analysis, glyph shaping)  │
/// └─────────────────────────────────────┘
///                ↓
/// ┌─────��───────────────────────────────┐
/// │   Font Rasterization Backend        │
/// │   (DirectWrite / FreeType / Core)   │
/// └─────────────────────────────────────┘

#include "lithium/beryl/types.hpp"
#include "lithium/beryl/font.hpp"
#include "lithium/beryl/glyph.hpp"
#include "lithium/beryl/text_layout.hpp"
#include "lithium/beryl/text_engine.hpp"
#include "lithium/beryl/backend.hpp"

namespace lithium::beryl {

/// Initialize the Beryl text engine with platform-appropriate backend
[[nodiscard]] bool initialize();

/// Shutdown the text engine
void shutdown();

/// Get the global text engine instance
[[nodiscard]] TextEngine* get_engine() noexcept;

/// Query engine capabilities
struct EngineCapabilities {
    bool supports_subpixel_positioning;
    bool supports_color_fonts;
    bool supports_variable_fonts;
    bool supports_ligatures;
    i32 max_glyph_texture_size;
};

[[nodiscard]] EngineCapabilities get_capabilities() noexcept;

} // namespace lithium::beryl
