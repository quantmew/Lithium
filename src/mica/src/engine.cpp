/**
 * Mica Graphics Engine - Engine Implementation
 */

#include "lithium/mica/mica.hpp"
#include "lithium/beryl/beryl.hpp"
#include "lithium/core/logger.hpp"

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace lithium::mica {

// ============================================================================
// Engine::Impl - Private Implementation
// ============================================================================

class Engine::Impl {
public:
    Impl() = default;

    ~Impl() {
        // Destroy backend
        m_backend.reset();
    }

    // Prevent copying
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Backend
    std::unique_ptr<IBackend> m_backend;
    BackendType m_backend_type{BackendType::Auto};

    // Capabilities
    BackendCapabilities m_capabilities;
};

// ============================================================================
// Engine Implementation
// ============================================================================

Engine::Engine()
    : m_impl(std::make_unique<Impl>())
{
    LITHIUM_LOG_INFO("Mica Engine: Constructor called");
    // Note: Text engine integration is optional for now
    // TODO: Initialize Beryl text engine when available
}

Engine::~Engine() {
    LITHIUM_LOG_INFO("Mica Engine: Destructor called");
}

Engine::Engine(Engine&&) noexcept = default;

Engine& Engine::operator=(Engine&&) noexcept = default;

bool Engine::initialize(BackendType type) {
    LITHIUM_LOG_INFO_FMT("Mica Engine: Initializing with backend type: {}", backend_type_name(type));

    // Register all available backends first
    register_available_backends();

    // Create backend
    m_impl->m_backend = create_backend(type);
    if (!m_impl->m_backend) {
        LITHIUM_LOG_ERROR("Mica Engine: Failed to create backend");
        return false;
    }

    m_impl->m_backend_type = m_impl->m_backend->type();
    m_impl->m_capabilities = m_impl->m_backend->capabilities();

    LITHIUM_LOG_INFO_FMT("Mica Engine: Backend created successfully: {}", backend_type_name(m_impl->m_backend_type));
    LITHIUM_LOG_INFO_FMT("Mica Engine: Supports multisampling: {}", m_impl->m_capabilities.supports_multisampling ? "Yes" : "No");
    LITHIUM_LOG_INFO_FMT("Mica Engine: Max texture size: {}", m_impl->m_capabilities.max_texture_size);

    // Note: Text engine integration is optional for now
    // TODO: Initialize Beryl text engine when available

    return true;
}

BackendType Engine::backend_type() const noexcept {
    return m_impl->m_backend_type;
}

std::unique_ptr<Context> Engine::create_context(void* native_window) {
    if (!m_impl->m_backend) {
        LITHIUM_LOG_ERROR("Mica Engine: No backend available");
        return nullptr;
    }

    NativeWindowHandle handle{};
    handle.handle = native_window;

#if defined(_WIN32)
    handle.hwnd = native_window;

    // Get actual window size
    HWND hwnd = static_cast<HWND>(native_window);
    RECT rect{};
    GetClientRect(hwnd, &rect);
    i32 window_width = rect.right - rect.left;
    i32 window_height = rect.bottom - rect.top;
#else
    i32 window_width = 1280;
    i32 window_height = 720;
#endif

    SwapChainConfig config{};
    config.width = window_width > 0 ? window_width : 1280;
    config.height = window_height > 0 ? window_height : 720;
    config.buffer_count = 2;
    config.vsync = true;

    auto context = m_impl->m_backend->create_context(handle, config);
    if (!context) {
        LITHIUM_LOG_ERROR("Mica Engine: Failed to create graphics context");
        return nullptr;
    }

    LITHIUM_LOG_INFO("Mica Engine: Graphics context created successfully");
    return context;
}

std::unique_ptr<Painter> Engine::create_painter(Context& context) {
    if (!context.backend()) {
        LITHIUM_LOG_ERROR("Mica Engine: Context has no backend");
        return nullptr;
    }

    auto painter = context.create_painter();
    if (!painter) {
        LITHIUM_LOG_ERROR("Mica Engine: Failed to create painter");
        return nullptr;
    }

    LITHIUM_LOG_INFO("Mica Engine: Painter created successfully");
    return painter;
}

Engine::Capabilities Engine::capabilities() const {
    if (!m_impl->m_backend) {
        return {false, false, false, 0, 0};
    }

    const auto& bc = m_impl->m_capabilities;
    return {
        bc.supports_multisampling,
        bc.supports_shaders,
        bc.supports_compute,
        bc.max_texture_size,
        bc.max_texture_units
    };
}

} // namespace lithium::mica
