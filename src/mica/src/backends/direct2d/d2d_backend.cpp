/**
 * Direct2D Backend for Mica Graphics Engine
 */

#include "d2d_backend.hpp"
#include "lithium/mica/backend.hpp"
#include <iostream>
#include <dwrite.h>   // For DirectWrite
#include <wincodec.h> // For WIC

namespace lithium::mica::direct2d {

// ============================================================================
// D2DBackend
// ============================================================================

D2DBackend::D2DBackend() {
    // Initialize capabilities
    m_capabilities.supports_multisampling = true;
    m_capabilities.supports_shaders = false;  // Direct2D doesn't use custom shaders
    m_capabilities.supports_compute = false;
    m_capabilities.supports_framebuffer_srgb = true;
    m_capabilities.max_texture_size = 16384;  // D3D11 limit
    m_capabilities.max_texture_units = 16;
    m_capabilities.max_render_targets = 1;  // Direct2D single render target
    m_capabilities.max_vertex_attributes = 0;  // Not applicable
    m_capabilities.max_uniform_buffer_bindings = 0;  // Not applicable
    m_capabilities.max_anisotropy = 16.0f;

    // MSAA sample counts supported by Direct2D
    m_capabilities.sample_counts = {1, 2, 4, 8};
}

D2DBackend::~D2DBackend() {
    // Release COM resources (ComPtr handles this automatically)
}

bool D2DBackend::initialize() {
    if (!initialize_factories()) {
        std::cerr << "D2DBackend: Failed to initialize Direct2D factories" << std::endl;
        return false;
    }

    if (!initialize_d3d()) {
        std::cerr << "D2DBackend: Failed to initialize Direct3D device" << std::endl;
        return false;
    }

    std::cout << "D2DBackend: Initialized successfully" << std::endl;
    return true;
}

bool D2DBackend::initialize_factories() {
    // Create Direct2D factory
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        nullptr,
        &m_d2d_factory
    );

    if (FAILED(hr)) {
        std::cerr << "D2DBackend: Failed to create Direct2D factory (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        &m_dwrite_factory
    );

    if (FAILED(hr)) {
        std::cerr << "D2DBackend: Failed to create DirectWrite factory (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    // Create WIC factory
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory),
        &m_wic_factory
    );

    if (FAILED(hr)) {
        std::cerr << "D2DBackend: Failed to create WIC factory (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    return true;
}

bool D2DBackend::initialize_d3d() {
    // Create Direct3D 11 device
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,  // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_d3d_device,
        nullptr,
        &m_d3d_context
    );

    if (FAILED(hr)) {
        std::cerr << "D2DBackend: Failed to create Direct3D device (HRESULT: 0x"
                  << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    return true;
}

std::unique_ptr<Context> D2DBackend::create_context(
    NativeWindowHandle window,
    const SwapChainConfig& config) {

    HWND hwnd = static_cast<HWND>(window.hwnd);
    if (!hwnd || !IsWindow(hwnd)) {
        std::cerr << "D2DBackend: Invalid window handle" << std::endl;
        return nullptr;
    }

    auto context = std::make_unique<D2DContext>(*this, hwnd, config);
    if (!context || !context->is_valid()) {
        std::cerr << "D2DBackend: Failed to create context" << std::endl;
        return nullptr;
    }

    return context;
}

std::unique_ptr<RenderTarget> D2DBackend::create_render_target(
    const RenderTargetDesc& desc) {

    // TODO: Implement off-screen render target creation
    std::cerr << "D2DBackend: Off-screen render targets not yet implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<Buffer> D2DBackend::create_buffer(
    BufferType type,
    BufferUsage usage,
    usize size,
    const void* data) {

    // Direct2D doesn't use explicit buffers like OpenGL
    // This is mainly for compatibility with the API
    std::cerr << "D2DBackend: Buffer creation not applicable for Direct2D" << std::endl;
    return nullptr;
}

std::unique_ptr<Shader> D2DBackend::create_shader(
    const String& vertex_source,
    const String& fragment_source) {

    // Direct2D doesn't support custom shaders (uses fixed-function pipeline)
    std::cerr << "D2DBackend: Custom shaders not supported by Direct2D" << std::endl;
    return nullptr;
}

void D2DBackend::flush() {
    if (m_d3d_context) {
        m_d3d_context->Flush();
    }
}

void D2DBackend::finish() {
    flush();
    // Add a sync if needed for strict synchronization
}

void register_direct2d_backend_factory() {
    mica::register_backend_factory(BackendType::Direct2D, []() -> std::unique_ptr<IBackend> {
        auto backend = std::make_unique<D2DBackend>();
        if (backend && backend->initialize()) {
            return backend;
        }
        return nullptr;
    });
}

} // namespace lithium::mica::direct2d
