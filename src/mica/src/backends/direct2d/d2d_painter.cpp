/**
 * Direct2D Painter Implementation
 */

#include "d2d_backend.hpp"
#include "lithium/beryl/types.hpp"
#include "lithium/beryl/text_layout.hpp"
#include <algorithm>
#include <iostream>
#include <windows.h>  // For MultiByteToWideChar
#include <dwrite.h>   // For DirectWrite

namespace lithium::mica::direct2d {

// ============================================================================
// D2DPainter
// ============================================================================

D2DPainter::D2DPainter(D2DContext& context)
    : m_context(context)
{
    // Initialize default state
    m_current_state.transform = Mat3::identity();
    m_current_state.line_width = 1.0f;
    m_current_state.line_cap = LineCap::Butt;
    m_current_state.line_join = LineJoin::Miter;
    m_current_state.miter_limit = 4.0f;
}

D2DPainter::~D2DPainter() = default;

Context* D2DPainter::context() noexcept {
    return &m_context;
}

void D2DPainter::save() {
    m_state_stack.push_back(m_current_state);
}

void D2DPainter::restore() {
    if (!m_state_stack.empty()) {
        m_current_state = m_state_stack.back();
        m_state_stack.pop_back();
        update_transform();
    }
}

const PainterState& D2DPainter::state() const noexcept {
    return m_current_state;
}

void D2DPainter::translate(Vec2 offset) {
    Mat3 translation = Mat3::translation(offset.x, offset.y);
    m_current_state.transform = translation * m_current_state.transform;
    update_transform();
}

void D2DPainter::scale(Vec2 factors) {
    Mat3 scaling = Mat3::scale(factors.x, factors.y);
    m_current_state.transform = scaling * m_current_state.transform;
    update_transform();
}

void D2DPainter::rotate(f32 angle) {
    Mat3 rotation = Mat3::rotation(angle);
    m_current_state.transform = rotation * m_current_state.transform;
    update_transform();
}

void D2DPainter::concat(const Mat3& matrix) {
    m_current_state.transform = matrix * m_current_state.transform;
    update_transform();
}

void D2DPainter::set_transform(const Mat3& transform) {
    m_current_state.transform = transform;
    update_transform();
}

const Mat3& D2DPainter::transform() const noexcept {
    return m_current_state.transform;
}

void D2DPainter::draw_line(Vec2 start, Vec2 end, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    ctx->DrawLine(
        D2D1::Point2F(start.x, start.y),
        D2D1::Point2F(end.x, end.y),
        brush.Get(),
        m_current_state.line_width
    );
}

void D2DPainter::draw_rect(const Rect& rect, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_RECT_F d2d_rect = D2D1::RectF(
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    ctx->DrawRectangle(
        &d2d_rect,
        brush.Get(),
        m_current_state.line_width
    );
}

void D2DPainter::fill_rect(const Rect& rect, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_RECT_F d2d_rect = D2D1::RectF(
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    ctx->FillRectangle(&d2d_rect, brush.Get());
}

void D2DPainter::draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_RECT_F d2d_rect = D2D1::RectF(
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
        d2d_rect,
        radius,
        radius
    );

    ctx->DrawRoundedRectangle(
        &rounded_rect,
        brush.Get(),
        m_current_state.line_width
    );
}

void D2DPainter::fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_RECT_F d2d_rect = D2D1::RectF(
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
        d2d_rect,
        radius,
        radius
    );

    ctx->FillRoundedRectangle(&rounded_rect, brush.Get());
}

void D2DPainter::draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F(center.x, center.y),
        radius_x,
        radius_y
    );

    ctx->DrawEllipse(
        &ellipse,
        brush.Get(),
        m_current_state.line_width
    );
}

void D2DPainter::fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F(center.x, center.y),
        radius_x,
        radius_y
    );

    ctx->FillEllipse(&ellipse, brush.Get());
}

void D2DPainter::draw_circle(Vec2 center, f32 radius, const Paint& paint) {
    draw_ellipse(center, radius, radius, paint);
}

void D2DPainter::fill_circle(Vec2 center, f32 radius, const Paint& paint) {
    fill_ellipse(center, radius, radius, paint);
}

void D2DPainter::draw_path(const Path& path, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    ComPtr<ID2D1Geometry> geometry = create_geometry(path);
    if (!geometry) return;

    ctx->DrawGeometry(
        geometry.Get(),
        brush.Get(),
        m_current_state.line_width
    );
}

void D2DPainter::fill_path(const Path& path, const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    ComPtr<ID2D1Geometry> geometry = create_geometry(path);
    if (!geometry) return;

    ctx->FillGeometry(geometry.Get(), brush.Get());
}

void D2DPainter::draw_text(
    Vec2 position,
    const String& text,
    const Paint& paint,
    const beryl::FontDescription& font_desc) {

    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    D2DBackend* backend = static_cast<D2DBackend*>(m_context.backend());
    IDWriteFactory* dwrite_factory = backend->dwrite_factory();
    if (!dwrite_factory) return;

    // Convert to wide string for DirectWrite
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return;
    std::wstring wide_text(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide_text[0], wide_len);

    // Convert font family to wide string
    int family_wide_len = MultiByteToWideChar(CP_UTF8, 0, font_desc.family.c_str(), -1, nullptr, 0);
    if (family_wide_len <= 0) return;
    std::wstring wide_family(family_wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, font_desc.family.c_str(), -1, &wide_family[0], family_wide_len);

    // Create text format
    ComPtr<IDWriteTextFormat> text_format;
    HRESULT hr = dwrite_factory->CreateTextFormat(
        wide_family.c_str(),
        nullptr,
        static_cast<DWRITE_FONT_WEIGHT>(static_cast<int>(font_desc.weight)),
        static_cast<DWRITE_FONT_STYLE>(static_cast<int>(font_desc.style)),
        static_cast<DWRITE_FONT_STRETCH>(static_cast<int>(font_desc.stretch)),
        font_desc.size,
        L"en-us",
        &text_format
    );

    if (FAILED(hr)) {
        std::cerr << "D2DPainter: Failed to create text format" << std::endl;
        return;
    }

    // Create brush
    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (!brush) return;

    // Draw text
    ctx->DrawText(
        wide_text.c_str(),
        static_cast<UINT32>(wide_text.length()),
        text_format.Get(),
        D2D1::RectF(
            position.x,
            position.y,
            position.x + 10000,  // Large value for no clipping
            position.y + 10000
        ),
        brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL
    );
}

void D2DPainter::draw_text_layout(
    Vec2 position,
    const beryl::TextLayout& layout,
    const Paint& paint) {

    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    // TODO: Implement proper text layout rendering
    // For now, this is a placeholder that draws a simple box
    // to indicate where the text layout would be rendered

    Size layout_size = layout.size();
    Rect rect = {
        position.x,
        position.y,
        layout_size.width,
        layout_size.height
    };

    // Draw a placeholder rectangle
    ComPtr<ID2D1Brush> brush = create_brush(paint);
    if (brush) {
        ctx->DrawRectangle(
            D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height),
            brush.Get(),
            1.0f
        );
    }
}

void D2DPainter::draw_image(Vec2 position, Texture* texture, const Paint& paint) {
    if (!texture) return;

    Rect dest_rect = {
        position.x,
        position.y,
        static_cast<f32>(texture->size().width),
        static_cast<f32>(texture->size().height)
    };

    Rect src_rect = {
        0,
        0,
        static_cast<f32>(texture->size().width),
        static_cast<f32>(texture->size().height)
    };

    draw_image_rect(dest_rect, texture, src_rect, paint);
}

void D2DPainter::draw_image_rect(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Paint& paint) {

    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx || !texture) return;

    // Get Direct2D bitmap from texture
    // TODO: Implement texture-to-bitmap conversion
    std::cerr << "D2DPainter: Image rendering not yet implemented" << std::endl;
}

void D2DPainter::draw_image_tinted(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Color& tint) {

    // TODO: Implement tinted image rendering
    std::cerr << "D2DPainter: Tinted image rendering not yet implemented" << std::endl;
}

void D2DPainter::clip_rect(const Rect& rect) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    D2D1_RECT_F d2d_rect = D2D1::RectF(
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    // Push axis-aligned clip layer
    ctx->PushAxisAlignedClip(&d2d_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void D2DPainter::clip_path(const Path& path) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ComPtr<ID2D1Geometry> geometry = create_geometry(path);
    if (!geometry) return;

    // Push geometry clip layer
    ctx->PushLayer(
        D2D1::LayerParameters1(),
        nullptr
    );
}

void D2DPainter::reset_clip() {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ctx->PopLayer();
    ctx->PopAxisAlignedClip();
}

void D2DPainter::clear(const Color& color) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    ctx->Clear(D2D1::ColorF(color.r, color.g, color.b, color.a));
}

// ============================================================================
// Helper Methods
// ============================================================================>

ComPtr<ID2D1Brush> D2DPainter::create_brush(const Paint& paint) {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return nullptr;

    if (!paint.brush) {
        // Create default solid brush
        ComPtr<ID2D1SolidColorBrush> solidBrush;
        HRESULT hr = ctx->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White),
            &solidBrush
        );
        if (SUCCEEDED(hr)) {
            return solidBrush;
        }
        return nullptr;
    }

    // Handle different brush types
    switch (paint.brush->type()) {
        case BrushType::Solid: {
            auto* solid = static_cast<SolidBrush*>(paint.brush.get());
            ComPtr<ID2D1SolidColorBrush> brush;
            HRESULT hr = ctx->CreateSolidColorBrush(
                D2D1::ColorF(solid->color.r, solid->color.g, solid->color.b, solid->color.a),
                &brush
            );
            if (SUCCEEDED(hr)) {
                return brush;
            }
            break;
        }

        // TODO: Implement gradient and image brushes
        default:
            std::cerr << "D2DPainter: Unsupported brush type" << std::endl;
            break;
    }

    return nullptr;
}

ComPtr<ID2D1Geometry> D2DPainter::create_geometry(const Path& path) {
    D2DBackend* backend = static_cast<D2DBackend*>(m_context.backend());
    ID2D1Factory1* factory = backend->d2d_factory();
    if (!factory) return nullptr;

    // TODO: Implement path-to-geometry conversion
    std::cerr << "D2DPainter: Path-to-geometry conversion not yet implemented" << std::endl;
    return nullptr;
}

void D2DPainter::update_transform() {
    ID2D1DeviceContext* ctx = m_context.d2d_context();
    if (!ctx) return;

    // Convert Mat3 to D2D1_MATRIX_3X2_F
    D2D1_MATRIX_3X2_F d2d_transform;
    d2d_transform._11 = m_current_state.transform.m[0][0];
    d2d_transform._12 = m_current_state.transform.m[0][1];
    d2d_transform._21 = m_current_state.transform.m[1][0];
    d2d_transform._22 = m_current_state.transform.m[1][1];
    d2d_transform._31 = m_current_state.transform.m[2][0];
    d2d_transform._32 = m_current_state.transform.m[2][1];

    ctx->SetTransform(d2d_transform);
}

} // namespace lithium::mica::direct2d
