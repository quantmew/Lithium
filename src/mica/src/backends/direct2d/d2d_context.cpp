/**
 * Direct2D Context Implementation
 */

#include "d2d_backend.hpp"
#include <iostream>

namespace lithium::mica::direct2d {

// ============================================================================
// D2DContext
// ============================================================================

D2DContext::D2DContext(D2DBackend& backend, HWND hwnd, const SwapChainConfig& config)
    : m_backend(backend)
    , m_config(config)
{
    m_window_handle.hwnd = hwnd;

    // Get DPI scale factor
    ID2D1Factory1* factory = m_backend.d2d_factory();
    FLOAT dpiX, dpiY;
    factory->GetDesktopDpi(&dpiX, &dpiY);
    m_dpi_scale = dpiX / 96.0f;

    // Create swap chain and back buffer
    if (!create_swap_chain()) {
        std::cerr << "D2DContext: Failed to create swap chain" << std::endl;
        return;
    }

    if (!create_back_buffer()) {
        std::cerr << "D2DContext: Failed to create back buffer" << std::endl;
        return;
    }

    std::cout << "D2DContext: Initialized successfully (DPI scale: " << m_dpi_scale << ")" << std::endl;
}

D2DContext::~D2DContext() {
    // Release resources (ComPtr handles this automatically)
}

SwapChain* D2DContext::swap_chain() noexcept {
    // For Direct2D, the swap chain is managed by the context itself
    // Return this as the swap chain interface
    return reinterpret_cast<SwapChain*>(this);
}

std::unique_ptr<Painter> D2DContext::create_painter() {
    return std::make_unique<D2DPainter>(*this);
}

RenderTarget* D2DContext::current_render_target() noexcept {
    // For Direct2D, the back buffer is the current render target
    return reinterpret_cast<RenderTarget*>(this);
}

void D2DContext::set_render_target(RenderTarget* target) {
    // TODO: Implement render target switching
    std::cerr << "D2DContext: Render target switching not yet implemented" << std::endl;
}

bool D2DContext::resize(i32 width, i32 height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    // Release old back buffer
    m_back_buffer.Reset();
    m_d2d_context->SetTarget(nullptr);

    // Resize swap chain
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swap_chain->GetDesc1(&desc);

    HRESULT hr = m_swap_chain->ResizeBuffers(
        desc.BufferCount,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        desc.Format,
        desc.Flags
    );

    if (FAILED(hr)) {
        std::cerr << "D2DContext: Failed to resize swap chain (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    // Recreate back buffer
    return create_back_buffer();
}

Size D2DContext::size() const noexcept {
    if (!m_swap_chain) {
        return {0, 0};
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swap_chain->GetDesc1(&desc);
    return {static_cast<f32>(desc.Width), static_cast<f32>(desc.Height)};
}

void D2DContext::begin_frame() {
    if (!m_d2d_context) {
        return;
    }

    // Begin drawing
    m_d2d_context->BeginDraw();

    // Set default transform (identity)
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
    m_d2d_context->SetTransform(transform);
}

void D2DContext::end_frame() {
    if (!m_d2d_context) {
        return;
    }

    // End drawing
    HRESULT hr = m_d2d_context->EndDraw();

    if (FAILED(hr) && hr != D2DERR_RECREATE_TARGET) {
        std::cerr << "D2DContext: EndDraw failed (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
    }
}

void D2DContext::present() {
    if (!m_swap_chain) {
        return;
    }

    // Present to screen
    UINT sync_interval = m_config.vsync ? 1 : 0;
    UINT present_flags = 0;

    HRESULT hr = m_swap_chain->Present(sync_interval, present_flags);

    if (FAILED(hr)) {
        std::cerr << "D2DContext: Present failed (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
    }
}

void D2DContext::flush() {
    if (m_d2d_context) {
        m_d2d_context->Flush();
    }
}

f32 D2DContext::dpi_scale() const noexcept {
    return m_dpi_scale;
}

bool D2DContext::is_valid() const noexcept {
    return m_d2d_context && m_back_buffer && m_swap_chain;
}

bool D2DContext::create_swap_chain() {
    HWND hwnd = static_cast<HWND>(m_window_handle.hwnd);
    if (!hwnd) {
        return false;
    }

    // Get DXGI device
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_backend.d3d_device()->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice);
    if (FAILED(hr)) {
        return false;
    }

    // Get DXGI adapter
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        return false;
    }

    // Get DXGI factory
    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory);
    if (FAILED(hr)) {
        return false;
    }

    // Create swap chain description
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_config.width;
    swapChainDesc.Height = m_config.height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = m_config.sample_count;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_config.buffer_count;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = m_config.srgb ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;

    // Create swap chain
    hr = dxgiFactory->CreateSwapChainForHwnd(
        m_backend.d3d_device(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &m_swap_chain
    );

    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool D2DContext::create_back_buffer() {
    // Get swap chain surface
    ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = m_swap_chain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);
    if (FAILED(hr)) {
        return false;
    }

    // Create Direct2D render target from back buffer
    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    // Get DPI
    ID2D1Factory1* factory = m_backend.d2d_factory();
    FLOAT dpiX, dpiY;
    factory->GetDesktopDpi(&dpiX, &dpiY);

    hr = m_d2d_context->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProps,
        &m_back_buffer
    );

    if (FAILED(hr)) {
        return false;
    }

    // Set as render target
    m_d2d_context->SetTarget(m_back_buffer.Get());

    return true;
}

} // namespace lithium::mica::direct2d
