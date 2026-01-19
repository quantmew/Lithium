/**
 * Graphics Backend Factory Implementation
 *
 * Note: Hardware graphics backends (OpenGL, Direct2D) are now provided by the mica module.
 * This factory now provides a basic software renderer and delegates to mica for hardware rendering.
 */

#include "lithium/platform/graphics_backend.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"

// Backend-specific includes (DEPRECATED - use mica instead)
// #ifdef LITHIUM_HAS_OPENGL
//     #include "lithium/platform/opengl_graphics_context.hpp"
// #endif
//
// #ifdef LITHIUM_HAS_DIRECT2D
//     #include "lithium/platform/d2d_graphics_context.hpp"
// #endif

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <d3d11.h>
    #include <dxgi.h>
#endif

// #ifdef LITHIUM_HAS_OPENGL
//     #include <GL/gl.h>
// #endif

#include <algorithm>

namespace lithium::platform {

// ============================================================================
// GraphicsBackendFactory Implementation
// ============================================================================

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create(Window* window, const GraphicsConfig& config) {
    if (!window) {
        return make_error(BackendError::InvalidConfig);
    }

    // Determine which backend to try
    GraphicsConfig::BackendType backend_to_try = config.preferred_backend;

    if (backend_to_try == GraphicsConfig::BackendType::Auto) {
        backend_to_try = default_backend();
    }

    // Try the requested backend
    auto result = try_create_backend(window, config, backend_to_try);

    // If failed and fallback is allowed, try software
    if (result.is_err() && config.allow_fallback) {
        if (backend_to_try != GraphicsConfig::BackendType::Software) {
            LITHIUM_LOG_WARN("Hardware backend initialization failed, falling back to software rendering");
            result = create_software(window);
        }
    }

    return result;
}

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::try_create_backend(
    Window* window,
    const GraphicsConfig& config,
    GraphicsConfig::BackendType backend
) {
    LITHIUM_LOG_INFO_FMT("try_create_backend: backend type = {}", static_cast<int>(backend));

    switch (backend) {
        case GraphicsConfig::BackendType::OpenGL:
            LITHIUM_LOG_WARN("OpenGL backend should use mica module, falling back to software");
            // OpenGL is now handled by mica - fall through to software for compatibility
            return create_software(window);

        case GraphicsConfig::BackendType::Direct2D:
            LITHIUM_LOG_WARN("Direct2D backend should use mica module, falling back to software");
            // Direct2D is now handled by mica - fall through to software for compatibility
            return create_software(window);

        case GraphicsConfig::BackendType::Software:
            LITHIUM_LOG_INFO_FMT("Creating software backend");
            return create_software(window);

        case GraphicsConfig::BackendType::Auto:
            // Should have been resolved before calling this function
            return make_error(BackendError::InvalidConfig);
    }

    return make_error(BackendError::InvalidConfig);
}

std::vector<GraphicsConfig::BackendType>
GraphicsBackendFactory::available_backends() {
    std::vector<GraphicsConfig::BackendType> backends;

    // Software is always available
    backends.push_back(GraphicsConfig::BackendType::Software);

    // Note: OpenGL and Direct2D are now provided by the mica module
    // The platform layer only provides the software renderer

    return backends;
}

BackendResult<GraphicsCapabilities>
GraphicsBackendFactory::query_capabilities(GraphicsConfig::BackendType backend) {
    GraphicsCapabilities caps;

    switch (backend) {
        case GraphicsConfig::BackendType::Software:
            caps.backend_name = "Software";
            caps.renderer_name = "CPU Software Rasterizer";
            caps.vendor_name = "Lithium";
            caps.version_string = "1.0.0";
            caps.hardware_accelerated = false;
            caps.supports_vsync = false;
            caps.supports_msaa = false;
            caps.supports_shaders = false;
            caps.max_texture_size = 4096;
            caps.max_texture_units = 1;
            break;

        case GraphicsConfig::BackendType::OpenGL:
        case GraphicsConfig::BackendType::Direct2D:
            // Hardware backends are provided by mica module
            return make_error(BackendError::NotSupported);

        case GraphicsConfig::BackendType::Auto:
            return make_error(BackendError::InvalidConfig);
    }

    return caps;
}

GraphicsConfig::BackendType
GraphicsBackendFactory::default_backend() {
    // Hardware backends (OpenGL, Direct2D) are provided by the mica module
    // The platform layer only provides the software renderer
    return GraphicsConfig::BackendType::Software;
}

// ============================================================================
// Platform-specific backend creation
// ============================================================================

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create_opengl(Window* window, const GraphicsConfig& config) {
    // OpenGL is now provided by the mica module
    (void)window;
    (void)config;
    LITHIUM_LOG_WARN("OpenGL backend should use mica module");
    return make_error(BackendError::NotSupported);
}

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create_direct2d(Window* window, const GraphicsConfig& config) {
    // Direct2D is now provided by the mica module
    (void)window;
    (void)config;
    LITHIUM_LOG_WARN("Direct2D backend should use mica module");
    return make_error(BackendError::NotSupported);
}

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create_software(Window* window) {
    // Software backend is always available
    // Use GraphicsContext::create which creates a software context
    auto context = GraphicsContext::create(window);
    if (context) {
        return context;
    }
    return make_error(BackendError::InitializationFailed);
}

// ============================================================================
// Backend availability checks (deprecated - use mica for hardware backends)
// ============================================================================

bool GraphicsBackendFactory::is_opengl_available() {
    // OpenGL is provided by the mica module
    return false;
}

bool GraphicsBackendFactory::is_direct2d_available() {
    // Direct2D is provided by the mica module
    return false;
}

} // namespace lithium::platform
