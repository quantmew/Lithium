/**
 * Direct2D Text Renderer Implementation
 *
 * Hardware-accelerated text rendering using DirectWrite + Direct2D.
 * Provides high-quality text rendering with subpixel anti-aliasing.
 */

#include "lithium/core/logger.hpp"
#include <unordered_map>
#include <string>
#include <cmath>

// Windows and DirectWrite headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <d2d1_1.h>
    #include <dwrite_1.h>
#endif

namespace lithium::platform {

using namespace lithium;

// ============================================================================
// DirectWrite Text Layout Cache
// ============================================================================

/**
 * @brief Cached text layout for efficient re-rendering
 */
struct CachedTextLayout {
    std::wstring text;
    std::wstring font_family;
    f32 size;
    f32 max_width;
    f32 max_height;

    #ifdef _WIN32
    IDWriteTextLayout* layout{nullptr};
    #endif

    void release() {
        #ifdef _WIN32
        if (layout) {
            layout->Release();
            layout = nullptr;
        }
        #endif
    }
};

// ============================================================================
// Direct2D Text Renderer Implementation
// ============================================================================

class D2DTextRendererImpl {
public:
    D2DTextRendererImpl() = default;

    ~D2DTextRendererImpl() {
        cleanup();
    }

    /**
     * @brief Initialize the text renderer with DirectWrite factory
     */
    [[nodiscard]] bool initialize(ID2D1Factory* d2d_factory) {
        #ifdef _WIN32
        // Get DirectWrite factory from Direct2D factory
        HRESULT hr = d2d_factory->GetParentInterface(
            __uuidof(IDWriteFactory),
            reinterpret_cast<void**>(&m_dwrite_factory)
        );

        if (FAILED(hr)) {
            // If we can't get it from D2D factory, create it manually
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&m_dwrite_factory)
            );

            if (FAILED(hr)) {
                LITHIUM_LOG_ERROR("Failed to create DirectWrite factory: 0x%X", hr);
                return false;
            }
        }

        // Create default text format
        hr = m_dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.0f,
            L"",
            &m_default_text_format
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create default text format: 0x%X", hr);
            return false;
        }

        LITHIUM_LOG_INFO("Direct2D Text Renderer initialized successfully");
        return true;
        #else
        (void)d2d_factory;
        return false;
        #endif
    }

    /**
     * @brief Draw text at the specified position
     */
    void draw_text(
        ID2D1DeviceContext* context,
        const PointF& position,
        const String& text,
        const Color& color,
        f32 size
    ) {
        #ifdef _WIN32
        if (!context || !m_dwrite_factory) {
            return;
        }

        // Convert UTF-8 to UTF-16 (Windows uses UTF-16)
        int wide_char_count = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            nullptr,
            0
        );

        if (wide_char_count <= 0) {
            return;
        }

        std::wstring wide_text(wide_char_count, L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            wide_text.data(),
            wide_char_count
        );

        // Create or get text format from cache
        IDWriteTextFormat* text_format = get_or_create_text_format(L"Segoe UI", size);
        if (!text_format) {
            LITHIUM_LOG_ERROR("Failed to create text format");
            return;
        }

        // Create solid color brush
        ID2D1SolidColorBrush* brush = nullptr;
        D2D1_COLOR_F d2d_color = {
            color.r / 255.0f,
            color.g / 255.0f,
            color.b / 255.0f,
            color.a / 255.0f
        };

        HRESULT hr = context->CreateSolidColorBrush(d2d_color, nullptr, &brush);
        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create brush: 0x%X", hr);
            return;
        }

        // Draw text
        D2D1_RECT_F layout_rect = {
            position.x,
            position.y,
            position.x + 10000.0f,  // Large max width
            position.y + 10000.0f   // Large max height
        };

        context->DrawText(
            wide_text.c_str(),
            static_cast<UINT32>(wide_text.length()),
            text_format,
            &layout_rect,
            brush,
            D2D1_DRAW_TEXT_OPTIONS_NONE,
            DWRITE_MEASURING_MODE_NATURAL
        );

        brush->Release();
        #else
        (void)context;
        (void)position;
        (void)text;
        (void)color;
        (void)size;
        #endif
    }

    /**
     * @brief Measure text width
     */
    [[nodiscard]] f32 measure_text(
        const String& text,
        const std::wstring& font_family,
        f32 size
    ) {
        #ifdef _WIN32
        if (!m_dwrite_factory) {
            return 0.0f;
        }

        // Convert UTF-8 to UTF-16
        int wide_char_count = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            nullptr,
            0
        );

        if (wide_char_count <= 0) {
            return 0.0f;
        }

        std::wstring wide_text(wide_char_count, L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            wide_text.data(),
            wide_char_count
        );

        // Create text format
        IDWriteTextFormat* text_format = get_or_create_text_format(font_family, size);
        if (!text_format) {
            return 0.0f;
        }

        // Create text layout for measuring
        IDWriteTextLayout* text_layout = nullptr;
        HRESULT hr = m_dwrite_factory->CreateTextLayout(
            wide_text.c_str(),
            static_cast<UINT32>(wide_text.length()),
            text_format,
            10000.0f,  // Max width
            10000.0f,  // Max height
            &text_layout
        );

        if (FAILED(hr) || !text_layout) {
            return 0.0f;
        }

        // Get metrics
        DWRITE_TEXT_METRICS metrics;
        hr = text_layout->GetMetrics(&metrics);

        text_layout->Release();

        if (FAILED(hr)) {
            return 0.0f;
        }

        return metrics.widthIncludingTrailingWhitespace;
        #else
        (void)text;
        (void)font_family;
        (void)size;
        return 0.0f;
        #endif
    }

    /**
     * @brief Get font metrics
     */
    [[nodiscard]] bool get_font_metrics(
        const std::wstring& font_family,
        f32 size,
        f32* ascent,
        f32* descent,
        f32* line_height
    ) {
        #ifdef _WIN32
        if (!m_dwrite_factory) {
            return false;
        }

        IDWriteTextFormat* text_format = get_or_create_text_format(font_family, size);
        if (!text_format) {
            return false;
        }

        // Get font metrics
        DWRITE_FONT_METRICS font_metrics;
        HRESULT hr = text_format->GetFontMetrics(&font_metrics);

        if (SUCCEEDED(hr)) {
            // Convert from design units to pixels
            float design_units_per_em = static_cast<float>(font_metrics.designUnitsPerEm);
            float pixels_per_design_unit = size / design_units_per_em;

            if (ascent) {
                *ascent = font_metrics.ascent * pixels_per_design_unit;
            }
            if (descent) {
                *descent = font_metrics.descent * pixels_per_design_unit;
            }
            if (line_height) {
                *line_height = (font_metrics.ascent + font_metrics.descent + font_metrics.lineGap) * pixels_per_design_unit;
            }
            return true;
        }

        return false;
        #else
        (void)font_family;
        (void)size;
        (void)ascent;
        (void)descent;
        (void)line_height;
        return false;
        #endif
    }

    /**
     * @brief Release all resources
     */
    void cleanup() {
        #ifdef _WIN32
        // Release cached text formats
        for (auto& pair : m_text_format_cache) {
            if (pair.second) {
                pair.second->Release();
            }
        }
        m_text_format_cache.clear();

        // Release cached layouts
        for (auto& layout : m_layout_cache) {
            layout.release();
        }
        m_layout_cache.clear();

        // Release default text format
        if (m_default_text_format) {
            m_default_text_format->Release();
            m_default_text_format = nullptr;
        }

        // Release DirectWrite factory
        if (m_dwrite_factory) {
            m_dwrite_factory->Release();
            m_dwrite_factory = nullptr;
        }
        #endif
    }

private:
    /**
     * @brief Get or create text format from cache
     */
    [[nodiscard]] IDWriteTextFormat* get_or_create_text_format(
        const std::wstring& font_family,
        f32 size
    ) {
        #ifdef _WIN32
        // Create cache key
        std::wstring cache_key = font_family + L"_" + std::to_wstring(static_cast<int>(size));

        // Check cache
        auto it = m_text_format_cache.find(cache_key);
        if (it != m_text_format_cache.end()) {
            return it->second;
        }

        // Create new text format
        IDWriteTextFormat* text_format = nullptr;
        HRESULT hr = m_dwrite_factory->CreateTextFormat(
            font_family.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"",
            &text_format
        );

        if (SUCCEEDED(hr) && text_format) {
            m_text_format_cache[cache_key] = text_format;
            return text_format;
        }

        // Fall back to default format if creation failed
        return m_default_text_format;
        #else
        (void)font_family;
        (void)size;
        return nullptr;
        #endif
    }

private:
    #ifdef _WIN32
    IDWriteFactory* m_dwrite_factory{nullptr};
    IDWriteTextFormat* m_default_text_format{nullptr};

    // Text format cache: key = "family_size", value = IDWriteTextFormat*
    std::unordered_map<std::wstring, IDWriteTextFormat*> m_text_format_cache;

    // Text layout cache for re-rendering
    std::vector<CachedTextLayout> m_layout_cache;
    #endif
};

// ============================================================================
// Public D2DTextRenderer Interface
// ============================================================================

D2DTextRenderer::D2DTextRenderer()
    : m_impl(std::make_unique<D2DTextRendererImpl>())
{
}

D2DTextRenderer::~D2DTextRenderer() = default;

void D2DTextRenderer::draw_text(
    ID2D1DeviceContext* context,
    const PointF& position,
    const String& text,
    const Color& color,
    f32 size
) {
    if (m_impl) {
        m_impl->draw_text(context, position, text, color, size);
    }
}

f32 D2DTextRenderer::measure_text(
    const String& text,
    const std::wstring& font_family,
    f32 size
) {
    if (m_impl) {
        return m_impl->measure_text(text, font_family, size);
    }
    return 0.0f;
}

bool D2DTextRenderer::get_font_metrics(
    const std::wstring& font_family,
    f32 size,
    f32* ascent,
    f32* descent,
    f32* line_height
) {
    if (m_impl) {
        return m_impl->get_font_metrics(font_family, size, ascent, descent, line_height);
    }
    return false;
}

bool D2DTextRenderer::initialize(ID2D1Factory* d2d_factory) {
    if (m_impl) {
        return m_impl->initialize(d2d_factory);
    }
    return false;
}

} // namespace lithium::platform
