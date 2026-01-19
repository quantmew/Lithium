/**
 * Software Context Implementation
 */

#include "sw_backend.hpp"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace lithium::mica::software {

// ============================================================================
// SoftwareContext
// ============================================================================

SoftwareContext::SoftwareContext(SoftwareBackend& backend, NativeWindowHandle window, const SwapChainConfig& config)
    : m_backend(backend)
    , m_window_handle(window)
    , m_config(config)
    , m_width(config.width)
    , m_height(config.height)
{
    // Get DPI scale
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(window.hwnd);
    HDC hdc = GetDC(hwnd);
    f32 dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
    m_dpi_scale = dpi_x / 96.0f;
    ReleaseDC(hwnd, hdc);
#endif

    if (!allocate_frame_buffer()) {
        std::cerr << "SoftwareContext: Failed to allocate frame buffer" << std::endl;
        return;
    }

    std::cout << "SoftwareContext: Initialized successfully (DPI scale: " << m_dpi_scale << ")" << std::endl;
}

SoftwareContext::~SoftwareContext() = default;

SwapChain* SoftwareContext::swap_chain() noexcept {
    return reinterpret_cast<SwapChain*>(this);
}

std::unique_ptr<Painter> SoftwareContext::create_painter() {
    return std::make_unique<SoftwarePainter>(*this);
}

RenderTarget* SoftwareContext::current_render_target() noexcept {
    return reinterpret_cast<RenderTarget*>(this);
}

void SoftwareContext::set_render_target(RenderTarget* target) {
    // TODO: Implement render target switching
}

bool SoftwareContext::resize(i32 width, i32 height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    m_width = width;
    m_height = height;

    return allocate_frame_buffer();
}

Size SoftwareContext::size() const noexcept {
    return {static_cast<f32>(m_width), static_cast<f32>(m_height)};
}

void SoftwareContext::begin_frame() {
    // Clear to black
    std::fill(m_frame_buffer.begin(), m_frame_buffer.end(), 0xFF000000);
}

void SoftwareContext::end_frame() {
    // Nothing to do
}

void SoftwareContext::present() {
#ifdef _WIN32
    // Blit frame buffer to window on Windows
    HWND hwnd = static_cast<HWND>(m_window_handle.hwnd);
    if (!hwnd) {
        return;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        return;
    }

    // Create a BITMAPINFOHEADER for the frame buffer
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = m_width;
    bi.biHeight = -m_height;  // Negative for top-down DIB
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // StretchDIBits to copy the frame buffer to the window
    StretchDIBits(
        hdc,
        0, 0, m_width, m_height,  // Destination rectangle
        0, 0, m_width, m_height,  // Source rectangle
        m_frame_buffer.data(),
        reinterpret_cast<BITMAPINFO*>(&bi),
        DIB_RGB_COLORS,
        SRCCOPY
    );

    ReleaseDC(hwnd, hdc);
#else
    // TODO: Implement for other platforms (Linux/X11, macOS/Cocoa)
    std::cerr << "SoftwareContext::present() not implemented for this platform" << std::endl;
#endif
}

void SoftwareContext::flush() {
    // Nothing to flush
}

f32 SoftwareContext::dpi_scale() const noexcept {
    return m_dpi_scale;
}

bool SoftwareContext::is_valid() const noexcept {
    return !m_frame_buffer.empty();
}

bool SoftwareContext::allocate_frame_buffer() {
    usize size = static_cast<usize>(m_width * m_height);
    if (size == 0) {
        return false;
    }

    try {
        m_frame_buffer.resize(size, 0xFF000000);  // Black with full alpha
        return true;
    } catch (const std::bad_alloc&) {
        std::cerr << "SoftwareContext: Failed to allocate frame buffer ("
                  << m_width << "x" << m_height << ")" << std::endl;
        return false;
    }
}

} // namespace lithium::mica::software
