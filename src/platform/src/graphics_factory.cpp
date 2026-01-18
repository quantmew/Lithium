/**
 * Graphics Backend Factory Implementation
 *
 * This file implements the factory pattern for creating graphics contexts
 * with automatic backend selection and fallback.
 */

#include "lithium/platform/graphics_backend.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"

// Backend-specific includes
#ifdef LITHIUM_HAS_OPENGL
    #include "lithium/platform/opengl_graphics_context.hpp"
#endif

#ifdef LITHIUM_HAS_DIRECT2D
    #include "lithium/platform/d2d_graphics_context.hpp"
#endif

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <d3d11.h>
    #include <dxgi.h>
#endif

#ifdef LITHIUM_HAS_OPENGL
    #include <GL/gl.h>
#endif

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
    LITHIUM_LOG_INFO("try_create_backend: backend type = {}", static_cast<int>(backend));

    switch (backend) {
        case GraphicsConfig::BackendType::OpenGL:
            LITHIUM_LOG_INFO("Trying OpenGL backend...");
            #ifdef LITHIUM_HAS_OPENGL
            if (is_opengl_available()) {
                LITHIUM_LOG_INFO("OpenGL is available, creating context");
                return create_opengl(window, config);
            }
            LITHIUM_LOG_WARN("OpenGL not available");
            #endif
            return make_error(BackendError::NotSupported);

        case GraphicsConfig::BackendType::Direct2D:
            LITHIUM_LOG_INFO("Trying Direct2D backend...");
            #ifdef LITHIUM_HAS_DIRECT2D
            LITHIUM_LOG_INFO("LITHIUM_HAS_DIRECT2D is defined");
            if (is_direct2d_available()) {
                LITHIUM_LOG_INFO("Direct2D is available, creating context");
                return create_direct2d(window, config);
            }
            LITHIUM_LOG_WARN("Direct2D not available");
            #else
            LITHIUM_LOG_ERROR("LITHIUM_HAS_DIRECT2D is NOT defined");
            #endif
            return make_error(BackendError::NotSupported);

        case GraphicsConfig::BackendType::Software:
            LITHIUM_LOG_INFO("Creating software backend");
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

    // Check OpenGL availability
    #ifdef LITHIUM_HAS_OPENGL
    if (is_opengl_available()) {
        backends.push_back(GraphicsConfig::BackendType::OpenGL);
    }
    #endif

    // Check Direct2D availability
    #ifdef LITHIUM_HAS_DIRECT2D
    if (is_direct2d_available()) {
        backends.push_back(GraphicsConfig::BackendType::Direct2D);
    }
    #endif

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
            #ifdef LITHIUM_HAS_OPENGL
            if (is_opengl_available()) {
                // Query OpenGL capabilities
                // This would require an active OpenGL context
                // For now, return basic info
                caps.backend_name = "OpenGL";
                caps.renderer_name = "OpenGL";
                caps.vendor_name = "Unknown";
                caps.version_string = "3.3+";
                caps.hardware_accelerated = true;
                caps.supports_vsync = true;
                caps.supports_msaa = true;
                caps.supports_shaders = true;
                caps.max_texture_size = 16384;
                caps.max_texture_units = 16;
                caps.max_msaa_samples = 8;
            } else {
                return make_error(BackendError::NotSupported);
            }
            #else
            return make_error(BackendError::NotSupported);
            #endif
            break;

        case GraphicsConfig::BackendType::Direct2D:
            #ifdef LITHIUM_HAS_DIRECT2D
            if (is_direct2d_available()) {
                caps.backend_name = "Direct2D";
                caps.renderer_name = "Direct2D 1.1";
                caps.vendor_name = "Microsoft";
                caps.version_string = "1.1";
                caps.hardware_accelerated = true;
                caps.supports_vsync = true;
                caps.supports_msaa = true;
                caps.supports_shaders = false; // Direct2D handles shaders internally
                caps.max_texture_size = 16384;
                caps.max_texture_units = 1;
                caps.max_msaa_samples = 8;
            } else {
                return make_error(BackendError::NotSupported);
            }
            #else
            return make_error(BackendError::NotSupported);
            #endif
            break;

        case GraphicsConfig::BackendType::Auto:
            return make_error(BackendError::InvalidConfig);
    }

    return caps;
}

GraphicsConfig::BackendType
GraphicsBackendFactory::default_backend() {
    #ifdef _WIN32
        // Windows: Prefer Direct2D, fallback to OpenGL
        #ifdef LITHIUM_HAS_DIRECT2D
        if (is_direct2d_available()) {
            return GraphicsConfig::BackendType::Direct2D;
        }
        #endif
        #ifdef LITHIUM_HAS_OPENGL
        if (is_opengl_available()) {
            return GraphicsConfig::BackendType::OpenGL;
        }
        #endif
    #else
        // Linux/macOS: Use OpenGL
        #ifdef LITHIUM_HAS_OPENGL
        if (is_opengl_available()) {
            return GraphicsConfig::BackendType::OpenGL;
        }
        #endif
    #endif

    // Fallback to software
    return GraphicsConfig::BackendType::Software;
}

// ============================================================================
// Platform-specific backend creation
// ============================================================================

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create_opengl(Window* window, const GraphicsConfig& config) {
    #ifdef LITHIUM_HAS_OPENGL
    auto context = OpenGLGraphicsContext::create(window, config);
    if (context) {
        return context;
    }
    return make_error(BackendError::InitializationFailed);
    #else
    (void)window;
    (void)config;
    return make_error(BackendError::NotSupported);
    #endif
}

BackendResult<std::unique_ptr<GraphicsContext>>
GraphicsBackendFactory::create_direct2d(Window* window, const GraphicsConfig& config) {
    #ifdef LITHIUM_HAS_DIRECT2D
    LITHIUM_LOG_INFO("Attempting to create Direct2D context...");
    auto context = D2DGraphicsContext::create(window, config);
    if (context) {
        LITHIUM_LOG_INFO("Direct2D context created successfully");
        return context;
    }
    LITHIUM_LOG_ERROR("Direct2D context creation returned nullptr");
    return make_error(BackendError::InitializationFailed);
    #else
    (void)window;
    (void)config;
    LITHIUM_LOG_ERROR("Direct2D support not compiled in");
    return make_error(BackendError::NotSupported);
    #endif
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
// Backend availability checks
// ============================================================================

bool GraphicsBackendFactory::is_opengl_available() {
    #ifdef LITHIUM_HAS_OPENGL
        #ifdef _WIN32
            // On Windows, check for OpenGL32.dll
            HMODULE module = GetModuleHandleA("opengl32.dll");
            return module != nullptr;
        #else
            // On Linux/macOS, assume OpenGL is available if compiled with support
            return true;
        #endif
    #else
        return false;
    #endif
}

bool GraphicsBackendFactory::is_direct2d_available() {
    #ifdef _WIN32
        // Check for D2D1.dll and D3D11.dll
        HMODULE d2d1_module = GetModuleHandleA("d2d1.dll");
        HMODULE d3d11_module = GetModuleHandleA("d3d11.dll");

        LITHIUM_LOG_INFO("Checking Direct2D availability:");
        LITHIUM_LOG_INFO("  d2d1.dll: {}", d2d1_module != nullptr ? "LOADED" : "NOT LOADED");
        LITHIUM_LOG_INFO("  d3d11.dll: {}", d3d11_module != nullptr ? "LOADED" : "NOT LOADED");

        bool available = d2d1_module != nullptr && d3d11_module != nullptr;
        LITHIUM_LOG_INFO("  Direct2D available: {}", available ? "YES" : "NO");
        return available;
    #else
        // Direct2D is Windows-only
        LITHIUM_LOG_INFO("Direct2D not available: not Windows platform");
        return false;
    #endif
}

} // namespace lithium::platform
