/**
 * DirectWrite Font Backend Implementation
 */

#include "dwrite_font.hpp"
#include "lithium/beryl/glyph.hpp"
#include <iostream>
#include <windows.h>
#include <vector>

namespace lithium::beryl::directwrite {

// ============================================================================
// DWriteFontBackend
// ============================================================================

DWriteFontBackend::DWriteFontBackend() {
    // Initialize capabilities
    m_capabilities.supports_subpixel_positioning = true;
    m_capabilities.supports_color_fonts = true;  // Windows 8.1+
    m_capabilities.supports_variable_fonts = true;  // Windows 10+
    m_capabilities.supports_ligatures = true;
    m_capabilities.supports_opentype_features = true;
    m_capabilities.supports_glyph_outline_extraction = true;
    m_capabilities.max_glyph_texture_size = 16384;

    // Supported antialiasing modes
    m_capabilities.antialiasing_modes = {
        TextAntialiasing::Grayscale,
        TextAntialiasing::Subpixel  // ClearType
    };
}

DWriteFontBackend::~DWriteFontBackend() = default;

bool DWriteFontBackend::initialize() {
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory1),
        &m_factory
    );

    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to create DirectWrite factory (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    std::cout << "DWriteFontBackend: Initialized successfully" << std::endl;
    return true;
}

std::unique_ptr<Font> DWriteFontBackend::load_font(
    const String& path,
    f32 size) {

    // TODO: Implement font loading from file
    std::cerr << "DWriteFontBackend: Font loading from file not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Font> DWriteFontBackend::load_font_from_memory(
    const void* data,
    usize size,
    f32 font_size) {

    // TODO: Implement font loading from memory
    std::cerr << "DWriteFontBackend: Font loading from memory not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Font> DWriteFontBackend::get_system_font(
    const FontDescription& desc) {

    // Convert family name to wide string
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, desc.family.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) {
        return nullptr;
    }
    std::wstring wide_family(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, desc.family.c_str(), -1, &wide_family[0], wide_len);

    // Create text format
    ComPtr<IDWriteTextFormat> text_format;
    HRESULT hr = m_factory->CreateTextFormat(
        wide_family.c_str(),
        nullptr,
        static_cast<DWRITE_FONT_WEIGHT>(static_cast<int>(desc.weight)),
        static_cast<DWRITE_FONT_STYLE>(static_cast<int>(desc.style)),
        static_cast<DWRITE_FONT_STRETCH>(static_cast<int>(desc.stretch)),
        desc.size,
        L"",
        &text_format
    );

    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to create text format for '"
                  << desc.family.c_str() << "'" << std::endl;
        return nullptr;
    }

    // Get font collection from system
    ComPtr<IDWriteFontCollection> font_collection;
    hr = m_factory->GetSystemFontCollection(&font_collection);
    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to get system font collection" << std::endl;
        return nullptr;
    }

    // Find family index
    UINT32 family_index;
    BOOL family_exists;
    hr = font_collection->FindFamilyName(wide_family.c_str(), &family_index, &family_exists);
    if (FAILED(hr) || !family_exists) {
        std::cerr << "DWriteFontBackend: Font family '" << desc.family.c_str()
                  << "' not found" << std::endl;
        return nullptr;
    }

    // Get font family
    ComPtr<IDWriteFontFamily> font_family;
    hr = font_collection->GetFontFamily(family_index, &font_family);
    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to get font family" << std::endl;
        return nullptr;
    }

    // Get font with matching properties
    ComPtr<IDWriteFont> font;
    hr = font_family->GetFirstMatchingFont(
        static_cast<DWRITE_FONT_WEIGHT>(static_cast<int>(desc.weight)),
        static_cast<DWRITE_FONT_STRETCH>(static_cast<int>(desc.stretch)),
        static_cast<DWRITE_FONT_STYLE>(static_cast<int>(desc.style)),
        &font
    );
    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to get matching font" << std::endl;
        return nullptr;
    }

    // Get font face
    ComPtr<IDWriteFontFace> font_face;
    hr = font->CreateFontFace(&font_face);
    if (FAILED(hr)) {
        std::cerr << "DWriteFontBackend: Failed to create font face" << std::endl;
        return nullptr;
    }

    // Get font metrics
    DWRITE_FONT_METRICS dw_metrics;
    font_face->GetMetrics(&dw_metrics);

    FontMetrics metrics;
    metrics.ascent = static_cast<f32>(dw_metrics.ascent) * desc.size / dw_metrics.designUnitsPerEm;
    metrics.descent = -static_cast<f32>(dw_metrics.descent) * desc.size / dw_metrics.designUnitsPerEm;
    metrics.line_gap = static_cast<f32>(dw_metrics.lineGap) * desc.size / dw_metrics.designUnitsPerEm;
    metrics.cap_height = static_cast<f32>(dw_metrics.capHeight) * desc.size / dw_metrics.designUnitsPerEm;
    metrics.x_height = static_cast<f32>(dw_metrics.xHeight) * desc.size / dw_metrics.designUnitsPerEm;
    metrics.units_per_em = static_cast<f32>(dw_metrics.designUnitsPerEm);

    return std::make_unique<DWriteFont>(font_face, desc, metrics);
}

std::unique_ptr<GlyphCache> DWriteFontBackend::create_glyph_cache(
    Font& font,
    i32 texture_size) {

    // TODO: Implement glyph cache
    std::cerr << "DWriteFontBackend: Glyph cache creation not yet implemented" << std::endl;
    return nullptr;
}

void DWriteFontBackend::set_rendering_mode(TextRenderingMode mode) {
    m_rendering_mode = mode;
}

void DWriteFontBackend::set_antialiasing(TextAntialiasing mode) {
    m_antialiasing = mode;
}

// ============================================================================
// DWriteFont
// ============================================================================

DWriteFont::DWriteFont(
    ComPtr<IDWriteFontFace> font_face,
    const FontDescription& desc,
    const FontMetrics& metrics)
    : m_font_face(std::move(font_face))
    , m_description(desc)
    , m_metrics(metrics)
{
}

GlyphMetrics DWriteFont::get_glyph_metrics(CodePoint cp) {
    GlyphMetrics metrics{};

    // Get glyph indices - convert u32 to UINT16
    UINT16 glyph_index;
    u32 temp_cp = cp;
    HRESULT hr = m_font_face->GetGlyphIndicesW(&temp_cp, 1, &glyph_index);
    if (FAILED(hr) || glyph_index == 0) {
        return metrics;  // Glyph not found
    }

    // Get glyph metrics
    DWRITE_GLYPH_METRICS dw_metrics;
    hr = m_font_face->GetDesignGlyphMetrics(
        &glyph_index,
        FALSE,
        &dw_metrics
    );

    if (SUCCEEDED(hr)) {
        f32 scale = m_description.size / m_metrics.units_per_em;
        metrics.glyph_id = glyph_index;
        metrics.advance = {
            static_cast<f32>(dw_metrics.advanceWidth) * scale,
            0
        };
        metrics.bearing = {
            static_cast<f32>(dw_metrics.leftSideBearing) * scale,
            static_cast<f32>(dw_metrics.topSideBearing) * scale
        };
        metrics.size = {
            static_cast<f32>(dw_metrics.advanceWidth) * scale,
            static_cast<f32>(dw_metrics.advanceHeight - dw_metrics.topSideBearing - dw_metrics.bottomSideBearing) * scale
        };
    }

    return metrics;
}

GlyphBitmap DWriteFont::rasterize_glyph(CodePoint cp) {
    GlyphBitmap bitmap{};

    // Get glyph indices - convert u32 to UINT16
    UINT16 glyph_index;
    u32 temp_cp = cp;
    HRESULT hr = m_font_face->GetGlyphIndicesW(&temp_cp, 1, &glyph_index);
    if (FAILED(hr) || glyph_index == 0) {
        return bitmap;
    }

    // TODO: Implement glyph rasterization using DirectWrite
    std::cerr << "DWriteFont: Glyph rasterization not yet implemented" << std::endl;

    return bitmap;
}

GlyphOutline DWriteFont::get_glyph_outline(CodePoint cp) {
    // TODO: Implement glyph outline extraction
    std::cerr << "DWriteFont: Glyph outline extraction not yet implemented" << std::endl;
    return {};
}

f32 DWriteFont::get_kerning(CodePoint left, CodePoint right) {
    // DirectWrite handles kerning automatically through the text layout engine.
    // There's no direct API to query kerning pairs between two glyphs.
    // For manual kerning queries, one would need to:
    // 1. Parse the GPOS table from the font file directly
    // 2. Use IDWriteTextLayout to measure text with and without the pair
    //
    // For now, return 0 as kerning will be handled by the text shaper.
    (void)left;
    (void)right;
    return 0.0f;
}

bool DWriteFont::has_glyph(CodePoint cp) const {
    // Get glyph indices - convert u32 to UINT16
    UINT16 glyph_index;
    u32 temp_cp = cp;
    HRESULT hr = const_cast<DWriteFont*>(this)->m_font_face->GetGlyphIndicesW(&temp_cp, 1, &glyph_index);
    return SUCCEEDED(hr) && glyph_index != 0;
}

std::vector<CodePoint> DWriteFont::get_supported_codepoints() const {
    // TODO: Implement codepoint enumeration
    std::cerr << "DWriteFont: Codepoint enumeration not yet implemented" << std::endl;
    return {};
}

f32 DWriteFont::measure_text(const String& text) {
    if (text.empty()) {
        return 0;
    }

    // Simple measurement using GetGlyphIndices and glyph metrics
    f32 total_width = 0;

    for (usize i = 0; i < text.size(); ++i) {
        CodePoint cp = text[i];
        GlyphMetrics metrics = const_cast<DWriteFont*>(this)->get_glyph_metrics(cp);
        total_width += metrics.advance.x;
    }

    return total_width;
}

f32 DWriteFont::measure_char(CodePoint cp) {
    GlyphMetrics metrics = const_cast<DWriteFont*>(this)->get_glyph_metrics(cp);
    return metrics.advance.x;
}

} // namespace lithium::beryl::directwrite
