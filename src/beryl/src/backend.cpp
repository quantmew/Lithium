/**
 * Beryl Text Engine - Font Backend Management
 */

#include "lithium/beryl/backend.hpp"
#include <unordered_map>
#include <iostream>

namespace lithium::beryl {

// ============================================================================
// Font Backend Detection
// ============================================================================

#if defined(_WIN32)
    #include <windows.h>

    FontBackendType get_preferred_font_backend() {
        // Windows: DirectWrite is preferred for ClearType support
        return FontBackendType::DirectWrite;
    }

#elif defined(__linux__) || defined(__ANDROID__)
    FontBackendType get_preferred_font_backend() {
        // Linux/Android: FreeType
        return FontBackendType::FreeType;
    }

#elif defined(__APPLE__)
    #include <TargetConditionals.h>

    FontBackendType get_preferred_font_backend() {
        #if TARGET_OS_IPHONE
            // iOS: Core Text
            return FontBackendType::CoreText;
        #else
            // macOS: Core Text
            return FontBackendType::CoreText;
        #endif
    }

#else
    FontBackendType get_preferred_font_backend() {
        // Unknown platform - this shouldn't happen
        std::cerr << "Beryl: Unknown platform, cannot determine font backend" << std::endl;
        return static_cast<FontBackendType>(-1);
    }
#endif

// ============================================================================
// Font Backend Registry
// ============================================================================

namespace {
    std::unordered_map<FontBackendType, FontBackendFactory> g_font_backend_factories;
}

void register_font_backend_factory(FontBackendType type, FontBackendFactory factory) {
    g_font_backend_factories[type] = std::move(factory);
    std::cout << "Beryl: Registered font backend factory for "
              << font_backend_name(type) << std::endl;
}

std::unique_ptr<IFontBackend> create_font_backend(FontBackendType type) {
    // Auto-detect if requested
    if (type == FontBackendType::Auto) {
        type = get_preferred_font_backend();
    }

    // Check if factory exists
    auto it = g_font_backend_factories.find(type);
    if (it != g_font_backend_factories.end()) {
        std::cout << "Beryl: Creating " << font_backend_name(type) << " font backend" << std::endl;
        return it->second();
    }

    std::cerr << "Beryl: Failed to create " << font_backend_name(type)
              << " font backend (no factory registered)" << std::endl;
    return nullptr;
}

std::unique_ptr<IFontBackend> initialize_default_font_backend() {
    FontBackendType preferred = get_preferred_font_backend();
    std::cout << "Beryl: Preferred font backend: " << font_backend_name(preferred) << std::endl;

    return create_font_backend(preferred);
}

// ============================================================================
// Script Detection
// ============================================================================

namespace {

// Unicode script ranges
struct ScriptRange {
    u32 start;
    u32 end;
    Script script;
};

// clang-format off
static const std::vector<ScriptRange> g_script_ranges = {
    // Latin
    {0x0020, 0x007F, Script::Common},
    {0x0080, 0x00FF, Script::Latin},
    {0x0100, 0x017F, Script::Latin},
    {0x1E00, 0x1EFF, Script::Latin},

    // Greek
    {0x0370, 0x03FF, Script::Greek},

    // Cyrillic
    {0x0400, 0x04FF, Script::Cyrillic},

    // Armenian
    {0x0531, 0x058F, Script::Armenian},

    // Hebrew
    {0x0591, 0x05F4, Script::Hebrew},

    // Arabic
    {0x0600, 0x06FF, Script::Arabic},
    {0x0750, 0x077F, Script::Arabic},

    // Devanagari
    {0x0900, 0x097F, Script::Devanagari},

    // Bengali
    {0x0980, 0x09FF, Script::Bengali},

    // Gurmukhi
    {0x0A00, 0x0A7F, Script::Gurmukhi},

    // Gujarati
    {0x0A80, 0x0AFF, Script::Gujarati},

    // Oriya
    {0x0B00, 0x0B7F, Script::Oriya},

    // Tamil
    {0x0B80, 0x0BFF, Script::Tamil},

    // Telugu
    {0x0C00, 0x0C7F, Script::Telugu},

    // Kannada
    {0x0C80, 0x0CFF, Script::Kannada},

    // Malayalam
    {0x0D00, 0x0D7F, Script::Malayalam},

    // Thai
    {0x0E00, 0x0E7F, Script::Thai},

    // Lao
    {0x0E80, 0x0EFF, Script::Lao},

    // Tibetan
    {0x0F00, 0x0FFF, Script::Tibetan},

    // Georgian
    {0x10A0, 0x10FF, Script::Georgian},

    // Hangul Jamo
    {0x1100, 0x11FF, Script::Hangul},

    // Ethiopic
    {0x1200, 0x137F, Script::Ethiopic},

    // Cherokee
    {0x13A0, 0x13FF, Script::Cherokee},

    // Han (CJK)
    {0x2E80, 0x9FFF, Script::Han},
    {0xF900, 0xFAFF, Script::Han},

    // Hiragana
    {0x3040, 0x309F, Script::Hiragana},

    // Katakana
    {0x30A0, 0x30FF, Script::Katakana},

    // Hangul Syllables
    {0xAC00, 0xD7AF, Script::Hangul},

    // CJK Symbols and Punctuation
    {0x3000, 0x303F, Script::Han},

    // Fullwidth Forms
    {0xFF00, 0xFFEF, Script::Han},
};
// clang-format on

} // anonymous namespace

Script detect_script(CodePoint cp) {
    // Binary search would be more efficient, but linear search is fine for now
    for (const auto& range : g_script_ranges) {
        if (cp >= range.start && cp <= range.end) {
            return range.script;
        }
    }

    return Script::Unknown;
}

// ============================================================================
// FontDescription Hash Implementation
// ============================================================================

usize FontDescription::hash() const {
    usize h = 0;
    // Hash the family string
    for (char c : family) {
        h = h * 31 + static_cast<usize>(c);
    }
    // Combine with other fields
    h ^= static_cast<usize>(size * 31);
    h ^= static_cast<usize>(weight) * 31;
    h ^= static_cast<usize>(style) * 31;
    h ^= static_cast<usize>(stretch) * 31;
    return h;
}

} // namespace lithium::beryl
