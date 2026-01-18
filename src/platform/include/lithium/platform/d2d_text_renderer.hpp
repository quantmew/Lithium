#pragma once

#include "lithium/core/types.hpp"
#include <memory>

#ifdef _WIN32
    // Forward declarations for Direct2D/DirectWrite types
    #ifndef _D2D1_H_
    typedef struct ID2D1Factory ID2D1Factory;
    #endif
    #ifndef _D2D1_1_H_
    typedef struct ID2D1DeviceContext ID2D1DeviceContext;
    #endif
#endif

namespace lithium::platform {

// ============================================================================
// Direct2D Text Renderer
// ============================================================================

/**
 * @brief Direct2D Text Renderer using DirectWrite for hardware-accelerated text rendering
 *
 * Provides high-quality text rendering with subpixel anti-aliasing using
 * DirectWrite and Direct2D on Windows.
 */
class D2DTextRenderer {
public:
    /**
     * @brief Constructor
     */
    D2DTextRenderer();

    /**
     * @brief Destructor
     */
    ~D2DTextRenderer();

    // Delete copy/move operations
    D2DTextRenderer(const D2DTextRenderer&) = delete;
    D2DTextRenderer& operator=(const D2DTextRenderer&) = delete;
    D2DTextRenderer(D2DTextRenderer&&) = delete;
    D2DTextRenderer& operator=(D2DTextRenderer&&) = delete;

    /**
     * @brief Initialize the text renderer with Direct2D factory
     * @param d2d_factory Direct2D factory instance
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize(ID2D1Factory* d2d_factory);

    /**
     * @brief Draw text at the specified position
     * @param context Direct2D device context
     * @param position Top-left corner of the text
     * @param text Text string to render (UTF-8)
     * @param color Text color
     * @param size Font size in pixels
     */
    void draw_text(
        ID2D1DeviceContext* context,
        const PointF& position,
        const String& text,
        const Color& color,
        f32 size
    );

    /**
     * @brief Measure text width
     * @param text Text string to measure (UTF-8)
     * @param font_family Font family name (UTF-16)
     * @param size Font size in pixels
     * @return Text width in pixels
     */
    [[nodiscard]] f32 measure_text(
        const String& text,
        const std::wstring& font_family,
        f32 size
    );

    /**
     * @brief Get font metrics
     * @param font_family Font family name (UTF-16)
     * @param size Font size in pixels
     * @param ascent Output: distance from baseline to top of em square
     * @param descent Output: distance from baseline to bottom of em square
     * @param line_height Output: total line height
     * @return true if metrics were retrieved successfully
     */
    [[nodiscard]] bool get_font_metrics(
        const std::wstring& font_family,
        f32 size,
        f32* ascent,
        f32* descent,
        f32* line_height
    );

private:
    // Pimpl pattern for implementation details
    class D2DTextRendererImpl;
    std::unique_ptr<D2DTextRendererImpl> m_impl;
};

} // namespace lithium::platform
