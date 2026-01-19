#pragma once

#include "lithium/mica/backend.hpp"
#include "lithium/mica/context.hpp"
#include "lithium/mica/painter.hpp"
#include "lithium/mica/resource.hpp"
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace lithium::mica::direct2d {

// ============================================================================
// Direct2D Backend
// ============================================================================

class D2DBackend : public IBackend {
public:
    D2DBackend();
    ~D2DBackend() override;

    // IBackend interface
    [[nodiscard]] BackendType type() const noexcept override {
        return BackendType::Direct2D;
    }

    [[nodiscard]] const BackendCapabilities& capabilities() const noexcept override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<Context> create_context(
        NativeWindowHandle window,
        const SwapChainConfig& config) override;

    [[nodiscard]] std::unique_ptr<RenderTarget> create_render_target(
        const RenderTargetDesc& desc) override;

    [[nodiscard]] std::unique_ptr<Texture> create_texture(
        i32 width,
        i32 height,
        ImageFormat format,
        const void* data,
        i32 stride) override;

    [[nodiscard]] std::unique_ptr<Buffer> create_buffer(
        BufferType type,
        BufferUsage usage,
        usize size,
        const void* data) override;

    [[nodiscard]] std::unique_ptr<Shader> create_shader(
        const String& vertex_source,
        const String& fragment_source) override;

    void flush() override;
    void finish() override;

    // Direct2D specific
    [[nodiscard]] ID2D1Factory1* d2d_factory() const noexcept {
        return m_d2d_factory.Get();
    }

    [[nodiscard]] IDWriteFactory* dwrite_factory() const noexcept {
        return m_dwrite_factory.Get();
    }

    [[nodiscard]] IWICImagingFactory* wic_factory() const noexcept {
        return m_wic_factory.Get();
    }

    [[nodiscard]] ID3D11Device* d3d_device() const noexcept {
        return m_d3d_device.Get();
    }

    [[nodiscard]] bool initialize();

private:
    BackendCapabilities m_capabilities;

    // Direct2D resources
    ComPtr<ID2D1Factory1> m_d2d_factory;
    ComPtr<IDWriteFactory> m_dwrite_factory;
    ComPtr<IWICImagingFactory> m_wic_factory;

    // Direct3D 11 resources (for hardware acceleration)
    ComPtr<ID3D11Device> m_d3d_device;
    ComPtr<ID3D11DeviceContext> m_d3d_context;

    // Initialize Direct2D factories
    [[nodiscard]] bool initialize_factories();

    // Initialize Direct3D 11 device
    [[nodiscard]] bool initialize_d3d();
};

// ============================================================================
// Direct2D Context
// ============================================================================

class D2DContext : public Context {
public:
    D2DContext(D2DBackend& backend, HWND hwnd, const SwapChainConfig& config);
    ~D2DContext() override;

    // Context interface
    [[nodiscard]] IBackend* backend() noexcept override {
        return &m_backend;
    }

    [[nodiscard]] NativeWindowHandle native_window() const noexcept override {
        return m_window_handle;
    }

    [[nodiscard]] SwapChain* swap_chain() noexcept override;
    [[nodiscard]] std::unique_ptr<Painter> create_painter() override;
    [[nodiscard]] RenderTarget* current_render_target() noexcept override;
    void set_render_target(RenderTarget* target) override;

    bool resize(i32 width, i32 height) override;
    [[nodiscard]] Size size() const noexcept override;

    void begin_frame() override;
    void end_frame() override;
    void present() override;
    void flush() override;

    [[nodiscard]] f32 dpi_scale() const noexcept override;
    [[nodiscard]] bool is_valid() const noexcept override;

    // Direct2D specific
    [[nodiscard]] ID2D1DeviceContext* d2d_context() const noexcept {
        return m_d2d_context.Get();
    }

    [[nodiscard]] IDXGISwapChain1* dxgi_swap_chain() const noexcept {
        return m_swap_chain.Get();
    }

private:
    D2DBackend& m_backend;
    NativeWindowHandle m_window_handle;
    SwapChainConfig m_config;

    // Direct2D render target
    ComPtr<ID2D1DeviceContext> m_d2d_context;
    ComPtr<ID2D1Bitmap1> m_back_buffer;

    // DXGI swap chain
    ComPtr<IDXGISwapChain1> m_swap_chain;

    // DPI scaling
    f32 m_dpi_scale{1.0f};

    [[nodiscard]] bool create_swap_chain();
    [[nodiscard]] bool create_back_buffer();
};

// ============================================================================
// Direct2D Painter
// ============================================================================>

class D2DPainter : public Painter {
public:
    D2DPainter(D2DContext& context);
    ~D2DPainter() override;

    // Painter interface
    [[nodiscard]] Context* context() noexcept override;
    void save() override;
    void restore() override;
    [[nodiscard]] const PainterState& state() const noexcept override;

    void translate(Vec2 offset) override;
    void scale(Vec2 factors) override;
    void rotate(f32 angle) override;
    void concat(const Mat3& matrix) override;
    void set_transform(const Mat3& transform) override;
    [[nodiscard]] const Mat3& transform() const noexcept override;

    void draw_line(Vec2 start, Vec2 end, const Paint& paint) override;
    void draw_rect(const Rect& rect, const Paint& paint) override;
    void fill_rect(const Rect& rect, const Paint& paint) override;
    void draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) override;
    void fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) override;
    void draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) override;
    void fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) override;
    void draw_circle(Vec2 center, f32 radius, const Paint& paint) override;
    void fill_circle(Vec2 center, f32 radius, const Paint& paint) override;
    void draw_path(const Path& path, const Paint& paint) override;
    void fill_path(const Path& path, const Paint& paint) override;

    void draw_text(Vec2 position, const String& text, const Paint& paint,
                  const beryl::FontDescription& font_desc) override;
    void draw_text_layout(Vec2 position, const beryl::TextLayout& layout,
                         const Paint& paint) override;

    void draw_image(Vec2 position, Texture* texture, const Paint& paint) override;
    void draw_image_rect(const Rect& dest, Texture* texture, const Rect& src,
                        const Paint& paint) override;
    void draw_image_tinted(const Rect& dest, Texture* texture, const Rect& src,
                          const Color& tint) override;

    void clip_rect(const Rect& rect) override;
    void clip_path(const Path& path) override;
    void reset_clip() override;

    void clear(const Color& color) override;

private:
    D2DContext& m_context;
    std::vector<PainterState> m_state_stack;
    PainterState m_current_state;

    // Direct2D specific helpers
    [[nodiscard]] ComPtr<ID2D1Brush> create_brush(const Paint& paint);
    [[nodiscard]] ComPtr<ID2D1Geometry> create_geometry(const Path& path);

    void update_transform();
};

// Register the Direct2D backend factory
void register_direct2d_backend_factory();

} // namespace lithium::mica::direct2d
