#pragma once

#include "lithium/beryl/backend.hpp"
#include "lithium/beryl/font.hpp"
#include <dwrite_1.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace lithium::beryl::directwrite {

// ============================================================================
// DirectWrite Font Backend
// ============================================================================

class DWriteFontBackend : public IFontBackend {
public:
    DWriteFontBackend();
    ~DWriteFontBackend() override;

    [[nodiscard]] FontBackendType type() const noexcept override {
        return FontBackendType::DirectWrite;
    }

    [[nodiscard]] const FontBackendCapabilities& capabilities() const noexcept override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<Font> load_font(
        const String& path,
        f32 size) override;

    [[nodiscard]] std::unique_ptr<Font> load_font_from_memory(
        const void* data,
        usize size,
        f32 font_size) override;

    [[nodiscard]] std::unique_ptr<Font> get_system_font(
        const FontDescription& desc) override;

    [[nodiscard]] std::unique_ptr<GlyphCache> create_glyph_cache(
        Font& font,
        i32 texture_size) override;

    void set_rendering_mode(TextRenderingMode mode) override;
    void set_antialiasing(TextAntialiasing mode) override;

    // DirectWrite specific
    [[nodiscard]] IDWriteFactory1* factory() const noexcept {
        return m_factory.Get();
    }

    [[nodiscard]] bool initialize();

private:
    FontBackendCapabilities m_capabilities;
    ComPtr<IDWriteFactory1> m_factory;
    TextRenderingMode m_rendering_mode{TextRenderingMode::Auto};
    TextAntialiasing m_antialiasing{TextAntialiasing::Default};
};

// ============================================================================
// DirectWrite Font
// ============================================================================

class DWriteFont : public Font {
public:
    DWriteFont(
        ComPtr<IDWriteFontFace> font_face,
        const FontDescription& desc,
        const FontMetrics& metrics);

    [[nodiscard]] const FontDescription& description() const noexcept override {
        return m_description;
    }

    [[nodiscard]] const FontMetrics& metrics() const noexcept override {
        return m_metrics;
    }

    [[nodiscard]] GlyphMetrics get_glyph_metrics(CodePoint cp) override;
    [[nodiscard]] GlyphBitmap rasterize_glyph(CodePoint cp) override;
    [[nodiscard]] GlyphOutline get_glyph_outline(CodePoint cp) override;

    [[nodiscard]] f32 get_kerning(CodePoint left, CodePoint right) override;
    [[nodiscard]] bool has_glyph(CodePoint cp) const override;
    [[nodiscard]] std::vector<CodePoint> get_supported_codepoints() const override;

    [[nodiscard]] f32 measure_text(const String& text) override;
    [[nodiscard]] f32 measure_char(CodePoint cp) override;

    [[nodiscard]] IFontBackend* backend() noexcept override {
        return m_backend;
    }

    // DirectWrite specific
    [[nodiscard]] IDWriteFontFace* font_face() const noexcept {
        return m_font_face.Get();
    }

private:
    ComPtr<IDWriteFontFace> m_font_face;
    FontDescription m_description;
    FontMetrics m_metrics;
    IFontBackend* m_backend{nullptr};
};

} // namespace lithium::beryl::directwrite
