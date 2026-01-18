/**
 * Direct2D Graphics Context Implementation
 *
 * Implements a hardware-accelerated rendering backend using Direct2D 1.1
 * and Direct3D 11.1 on Windows.
 */

#include "lithium/platform/d2d_graphics_context.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"

#include <algorithm>
#include <cstring>

// Windows headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <d3d11.h>
    #include <d2d1_1.h>
    #include <dxgi1_2.h>
#endif

namespace lithium::platform {

// ============================================================================
// Direct2D Render State (simplified internal implementation)
// ============================================================================

class D2DRenderState {
public:
    D2DRenderState() = default;
    ~D2DRenderState() = default;

    void set_transform(const f32 matrix[6]) {
        // TODO: Implement transform
        (void)matrix;
    }

    void push_clip(const RectF& rect) {
        // TODO: Implement clip layer
        (void)rect;
    }

    void pop_clip() {
        // TODO: Implement pop layer
    }

private:
};

// ============================================================================
// Direct2D Texture Cache (simplified internal implementation)
// ============================================================================

class D2DTextureCache {
public:
    D2DTextureCache() = default;
    ~D2DTextureCache() = default;

    ID2D1Bitmap* upload_bitmap(const Bitmap& bitmap, ID2D1DeviceContext* context) {
        // TODO: Implement texture upload
        (void)bitmap;
        (void)context;
        return nullptr;
    }

    void release_bitmap(ID2D1Bitmap* bitmap) {
        // TODO: Implement bitmap release
        (void)bitmap;
    }

private:
};

// ============================================================================
// Direct2D Text Renderer (simplified internal implementation)
// ============================================================================

class D2DTextRenderer {
public:
    D2DTextRenderer() = default;
    ~D2DTextRenderer() = default;

    void draw_text(
        ID2D1DeviceContext* context,
        const PointF& position,
        const String& text,
        const Color& color,
        f32 size
    ) {
        // TODO: Implement text rendering with FreeType
        (void)context;
        (void)position;
        (void)text;
        (void)color;
        (void)size;
    }

private:
};

// ============================================================================
// D2DGraphicsContext Implementation
// ============================================================================

std::unique_ptr<D2DGraphicsContext>
D2DGraphicsContext::create(Window* window, const GraphicsConfig& config) {
    if (!window) {
        LITHIUM_LOG_ERROR("Invalid window pointer");
        return nullptr;
    }

    #ifdef _WIN32
        // Direct2D is Windows-only
        auto context = std::unique_ptr<D2DGraphicsContext>(
            new D2DGraphicsContext(window, config)
        );

        if (!context->initialize(config)) {
            LITHIUM_LOG_ERROR("Failed to initialize Direct2D context");
            return nullptr;
        }

        return context;
    #else
        LITHIUM_LOG_ERROR("Direct2D is only supported on Windows");
        (void)config;
        return nullptr;
    #endif
}

D2DGraphicsContext::D2DGraphicsContext(
    Window* window,
    const GraphicsConfig& config
)
    : m_window(window)
    , m_vsync_enabled(config.enable_vsync)
    , m_msaa_samples(config.msaa_samples)
{
}

D2DGraphicsContext::~D2DGraphicsContext() {
    cleanup();
}

bool D2DGraphicsContext::initialize(const GraphicsConfig& config) {
    #ifdef _WIN32
        // Create Direct2D factory
        if (!create_d2d_factory()) {
            LITHIUM_LOG_ERROR("Failed to create Direct2D factory");
            return false;
        }

        // Create Direct3D device
        if (!create_d3d_device()) {
            LITHIUM_LOG_ERROR("Failed to create Direct3D device");
            return false;
        }

        // Create Direct2D device
        if (!create_d2d_device()) {
            LITHIUM_LOG_ERROR("Failed to create Direct2D device");
            return false;
        }

        // Create swap chain
        if (!create_swap_chain()) {
            LITHIUM_LOG_ERROR("Failed to create swap chain");
            return false;
        }

        // Create back buffer
        if (!create_back_buffer()) {
            LITHIUM_LOG_ERROR("Failed to create back buffer");
            return false;
        }

        // Create state managers
        m_render_state = std::make_unique<D2DRenderState>();
        m_texture_cache = std::make_unique<D2DTextureCache>();
        m_text_renderer = std::make_unique<D2DTextRenderer>();

        // Initialize transform
        m_current_transform = {0, 0, 1, 1, 0, {}};
        std::memset(m_current_transform.matrix, 0, sizeof(m_current_transform.matrix));
        m_current_transform.matrix[0] = 1.0f;  // Identity matrix
        m_current_transform.matrix[3] = 1.0f;

        // Setup initial viewport
        auto size = m_window->framebuffer_size();
        m_viewport = {0, 0, size.width, size.height};

        (void)config;
        return true;
    #else
        (void)config;
        return false;
    #endif
}

void D2DGraphicsContext::cleanup() {
    #ifdef _WIN32
        // Release Direct2D objects
        if (m_back_buffer) {
            static_cast<IUnknown*>(m_back_buffer)->Release();
            m_back_buffer = nullptr;
        }

        if (m_d2d_context) {
            static_cast<IUnknown*>(m_d2d_context)->Release();
            m_d2d_context = nullptr;
        }

        if (m_d2d_device) {
            static_cast<IUnknown*>(m_d2d_device)->Release();
            m_d2d_device = nullptr;
        }

        if (m_d2d_factory) {
            static_cast<IUnknown*>(m_d2d_factory)->Release();
            m_d2d_factory = nullptr;
        }

        // Release Direct3D objects
        if (m_swap_chain) {
            static_cast<IUnknown*>(m_swap_chain)->Release();
            m_swap_chain = nullptr;
        }

        if (m_d3d_context) {
            static_cast<IUnknown*>(m_d3d_context)->Release();
            m_d3d_context = nullptr;
        }

        if (m_d3d_device) {
            static_cast<IUnknown*>(m_d3d_device)->Release();
            m_d3d_device = nullptr;
        }
    #endif
}

bool D2DGraphicsContext::create_d2d_factory() {
    #ifdef _WIN32
        typedef HRESULT (WINAPI *D2D1CreateFactoryFunc)(
            D2D1_FACTORY_TYPE,
            REFIID,
            const D2D1_FACTORY_OPTIONS*,
            void**
        );

        HMODULE d2d1_module = LoadLibraryA("d2d1.dll");
        if (!d2d1_module) {
            LITHIUM_LOG_ERROR("Failed to load d2d1.dll");
            return false;
        }

        D2D1CreateFactoryFunc D2D1CreateFactory = (D2D1CreateFactoryFunc)GetProcAddress(
            d2d1_module, "D2D1CreateFactory"
        );

        if (!D2D1CreateFactory) {
            LITHIUM_LOG_ERROR("Failed to get D2D1CreateFactory function");
            FreeLibrary(d2d1_module);
            return false;
        }

        HRESULT hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            nullptr,
            reinterpret_cast<void**>(&m_d2d_factory)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("D2D1CreateFactory failed with HRESULT: 0x%X", hr);
            return false;
        }

        return true;
    #else
        return false;
    #endif
}

bool D2DGraphicsContext::create_d3d_device() {
    #ifdef _WIN32
        typedef HRESULT (WINAPI *D3D11CreateDeviceFunc)(
            IDXGIAdapter*,
            D3D_DRIVER_TYPE,
            HMODULE,
            UINT,
            const D3D_FEATURE_LEVEL*,
            UINT,
            UINT,
            ID3D11Device**,
            D3D_FEATURE_LEVEL*,
            ID3D11DeviceContext**
        );

        HMODULE d3d11_module = LoadLibraryA("d3d11.dll");
        if (!d3d11_module) {
            LITHIUM_LOG_ERROR("Failed to load d3d11.dll");
            return false;
        }

        D3D11CreateDeviceFunc D3D11CreateDevice = (D3D11CreateDeviceFunc)GetProcAddress(
            d3d11_module, "D3D11CreateDevice"
        );

        if (!D3D11CreateDevice) {
            LITHIUM_LOG_ERROR("Failed to get D3D11CreateDevice function");
            FreeLibrary(d3d11_module);
            return false;
        }

        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDevice(
            nullptr,  // Default adapter
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,  // No software rasterizer
            0,        // No flags
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            reinterpret_cast<ID3D11Device**>(&m_d3d_device),
            &feature_level,
            reinterpret_cast<ID3D11DeviceContext**>(&m_d3d_context)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("D3D11CreateDevice failed with HRESULT: 0x%X", hr);
            return false;
        }

        LITHIUM_LOG_INFO("Direct3D device created with feature level: 0x%X", feature_level);
        return true;
    #else
        return false;
    #endif
}

bool D2DGraphicsContext::create_d2d_device() {
    #ifdef _WIN32
        if (!m_d2d_factory || !m_d3d_device) {
            return false;
        }

        // Get DXGI device from Direct3D device
        IDXGIDevice* dxgi_device = nullptr;
        HRESULT hr = reinterpret_cast<ID3D11Device*>(m_d3d_device)->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(&dxgi_device)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to query DXGI device");
            return false;
        }

        // Create Direct2D device
        hr = reinterpret_cast<ID2D1Factory1*>(m_d2d_factory)->CreateDevice(
            dxgi_device,
            reinterpret_cast<ID2D1Device**>(&m_d2d_device)
        );

        dxgi_device->Release();

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create Direct2D device");
            return false;
        }

        // Create Direct2D device context
        hr = reinterpret_cast<ID2D1Device*>(m_d2d_device)->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            reinterpret_cast<ID2D1DeviceContext**>(&m_d2d_context)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create Direct2D device context");
            return false;
        }

        return true;
    #else
        return false;
    #endif
}

bool D2DGraphicsContext::create_swap_chain() {
    #ifdef _WIN32
        if (!m_d3d_device || !m_window) {
            return false;
        }

        // Get DXGI factory
        IDXGIDevice* dxgi_device = nullptr;
        HRESULT hr = reinterpret_cast<ID3D11Device*>(m_d3d_device)->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(&dxgi_device)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to query DXGI device");
            return false;
        }

        IDXGIAdapter* dxgi_adapter = nullptr;
        hr = dxgi_device->GetAdapter(&dxgi_adapter);
        dxgi_device->Release();

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to get DXGI adapter");
            return false;
        }

        IDXGIFactory2* dxgi_factory = nullptr;
        hr = dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgi_factory));
        dxgi_adapter->Release();

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to get DXGI factory");
            return false;
        }

        // Create swap chain
        auto size = m_window->framebuffer_size();

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.Width = size.width;
        swap_chain_desc.Height = size.height;
        swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swap_chain_desc.Stereo = FALSE;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swap_chain_desc.Flags = 0;

        HWND hwnd = static_cast<HWND>(m_window->native_handle());
        hr = dxgi_factory->CreateSwapChainForHwnd(
            reinterpret_cast<IUnknown*>(m_d3d_device),
            hwnd,
            &swap_chain_desc,
            nullptr,
            nullptr,
            reinterpret_cast<IDXGISwapChain1**>(&m_swap_chain)
        );

        dxgi_factory->Release();

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create swap chain");
            return false;
        }

        return true;
    #else
        return false;
    #endif
}

bool D2DGraphicsContext::create_back_buffer() {
    #ifdef _WIN32
        if (!m_swap_chain || !m_d2d_context) {
            return false;
        }

        // Get swap chain surface
        IDXGISurface* surface = nullptr;
        HRESULT hr = reinterpret_cast<IDXGISwapChain1*>(m_swap_chain)->GetBuffer(
            0,
            __uuidof(IDXGISurface),
            reinterpret_cast<void**>(&surface)
        );

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to get swap chain buffer");
            return false;
        }

        // Create bitmap from surface
        D2D1_BITMAP_PROPERTIES1 bitmap_props = {};
        bitmap_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        bitmap_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        bitmap_props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

        hr = reinterpret_cast<ID2D1DeviceContext*>(m_d2d_context)->CreateBitmapFromDxgiSurface(
            surface,
            &bitmap_props,
            reinterpret_cast<ID2D1Bitmap1**>(&m_back_buffer)
        );

        surface->Release();

        if (FAILED(hr)) {
            LITHIUM_LOG_ERROR("Failed to create back buffer bitmap");
            return false;
        }

        // Set as render target
        reinterpret_cast<ID2D1DeviceContext*>(m_d2d_context)->SetTarget(
            reinterpret_cast<ID2D1Bitmap*>(m_back_buffer)
        );

        return true;
    #else
        return false;
    #endif
}

void D2DGraphicsContext::handle_resize() {
    #ifdef _WIN32
        if (!m_swap_chain || !m_d2d_context) {
            return;
        }

        // Release back buffer
        if (m_back_buffer) {
            reinterpret_cast<ID2D1DeviceContext*>(m_d2d_context)->SetTarget(nullptr);
            static_cast<IUnknown*>(m_back_buffer)->Release();
            m_back_buffer = nullptr;
        }

        // Resize swap chain
        auto size = m_window->framebuffer_size();
        IDXGISwapChain* swap_chain = nullptr;
        HRESULT hr = reinterpret_cast<IDXGISwapChain1*>(m_swap_chain)->QueryInterface(
            __uuidof(IDXGISwapChain),
            reinterpret_cast<void**>(&swap_chain)
        );

        if (SUCCEEDED(hr) && swap_chain) {
            hr = swap_chain->ResizeBuffers(
                0,
                size.width,
                size.height,
                DXGI_FORMAT_UNKNOWN,
                0
            );
            swap_chain->Release();

            if (SUCCEEDED(hr)) {
                create_back_buffer();
            }
        }

        m_viewport = {0, 0, size.width, size.height};
    #endif
}

// ============================================================================
// Context Management
// ============================================================================

void D2DGraphicsContext::make_current() {
    // Direct2D doesn't have a concept of "current" context like OpenGL
    // The device context is implicitly current
}

void D2DGraphicsContext::swap_buffers() {
    #ifdef _WIN32
        if (m_swap_chain) {
            IDXGISwapChain* swap_chain = nullptr;
            HRESULT hr = reinterpret_cast<IDXGISwapChain1*>(m_swap_chain)->QueryInterface(
                __uuidof(IDXGISwapChain),
                reinterpret_cast<void**>(&swap_chain)
            );

            if (SUCCEEDED(hr) && swap_chain) {
                hr = swap_chain->Present(
                    m_vsync_enabled ? 1 : 0,
                    0
                );
                swap_chain->Release();

                if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                    handle_device_loss();
                }
            }
        }
    #endif
}

void D2DGraphicsContext::begin_frame() {
    m_in_frame = true;
    m_frame_counter++;

    // Check for resize
    auto size = m_window->framebuffer_size();
    if (size.width != m_viewport.width || size.height != m_viewport.height) {
        handle_resize();
    }
}

void D2DGraphicsContext::end_frame() {
    m_in_frame = false;
}

// ============================================================================
// Drawing Operations
// ============================================================================

void D2DGraphicsContext::clear(const Color& color) {
    #ifdef _WIN32
        if (m_d2d_context) {
            ID2D1DeviceContext* context = reinterpret_cast<ID2D1DeviceContext*>(m_d2d_context);

            // Create solid color brush for clear
            ID2D1SolidColorBrush* brush = nullptr;
            D2D1_COLOR_F d2d_color = {
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f
            };

            HRESULT hr = context->CreateSolidColorBrush(
                d2d_color,
                nullptr,
                &brush
            );

            if (SUCCEEDED(hr)) {
                D2D1_RECT_F rect = {
                    0.0f, 0.0f,
                    static_cast<f32>(m_viewport.width),
                    static_cast<f32>(m_viewport.height)
                };

                context->BeginDraw();
                context->FillRectangle(rect, brush);
                context->EndDraw();

                brush->Release();
            }
        }
    #else
        (void)color;
    #endif
}

void D2DGraphicsContext::fill_rect(const RectF& rect, const Color& color) {
    // TODO: Implement with Direct2D FillRectangle
    (void)rect;
    (void)color;
}

void D2DGraphicsContext::stroke_rect(const RectF& rect, const Color& color, f32 width) {
    // TODO: Implement with Direct2D DrawRectangle
    (void)rect;
    (void)color;
    (void)width;
}

void D2DGraphicsContext::draw_line(const PointF& from, const PointF& to, const Color& color, f32 width) {
    // TODO: Implement with Direct2D DrawLine
    (void)from;
    (void)to;
    (void)color;
    (void)width;
}

void D2DGraphicsContext::draw_text(const PointF& position, const String& text, const Color& color, f32 size) {
    #ifdef _WIN32
        if (m_text_renderer && m_d2d_context) {
            m_text_renderer->draw_text(
                reinterpret_cast<ID2D1DeviceContext*>(m_d2d_context),
                position,
                text,
                color,
                size
            );
        }
    #else
        (void)position;
        (void)text;
        (void)color;
        (void)size;
    #endif
}

void D2DGraphicsContext::draw_bitmap(const RectF& dest, const Bitmap& bitmap) {
    draw_bitmap(dest, {0, 0, static_cast<f32>(bitmap.width), static_cast<f32>(bitmap.height)}, bitmap);
}

void D2DGraphicsContext::draw_bitmap(const RectF& dest, const RectF& src, const Bitmap& bitmap) {
    // TODO: Implement with Direct2D DrawBitmap
    (void)dest;
    (void)src;
    (void)bitmap;
}

// ============================================================================
// State Management
// ============================================================================

void D2DGraphicsContext::push_clip(const RectF& rect) {
    if (!m_clip_stack.empty()) {
        // TODO: Intersect with current clip
    }

    m_clip_stack.push(rect);

    if (m_render_state) {
        m_render_state->push_clip(rect);
    }
}

void D2DGraphicsContext::pop_clip() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop();
    }

    if (m_render_state) {
        m_render_state->pop_clip();
    }
}

void D2DGraphicsContext::push_transform() {
    m_transform_stack.push(m_current_transform);
}

void D2DGraphicsContext::pop_transform() {
    if (!m_transform_stack.empty()) {
        m_current_transform = m_transform_stack.top();
        m_transform_stack.pop();

        if (m_render_state) {
            m_render_state->set_transform(m_current_transform.matrix);
        }
    }
}

void D2DGraphicsContext::translate(f32 x, f32 y) {
    m_current_transform.x += x;
    m_current_transform.y += y;
}

void D2DGraphicsContext::scale(f32 x, f32 y) {
    m_current_transform.scale_x *= x;
    m_current_transform.scale_y *= y;
}

void D2DGraphicsContext::rotate(f32 radians) {
    m_current_transform.rotation += radians;
}

void D2DGraphicsContext::push_opacity(f32 opacity) {
    m_opacity_stack.push(m_current_opacity);
    m_current_opacity *= opacity;
}

void D2DGraphicsContext::pop_opacity() {
    if (!m_opacity_stack.empty()) {
        m_current_opacity = m_opacity_stack.top();
        m_opacity_stack.pop();
    }
}

// ============================================================================
// Viewport
// ============================================================================

SizeI D2DGraphicsContext::viewport_size() const {
    return {m_viewport.width, m_viewport.height};
}

void D2DGraphicsContext::set_viewport(const RectI& rect) {
    m_viewport = rect;
}

bool D2DGraphicsContext::handle_device_loss() {
    // TODO: Implement device loss recovery
    LITHIUM_LOG_WARN("Direct2D device loss detected, attempting recovery");
    return false;
}

} // namespace lithium::platform
