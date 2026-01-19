/**
 * Mica Graphics Engine - Backend Management
 */

#include "lithium/mica/backend.hpp"

// Forward declarations for backend registration functions
namespace lithium::mica::software {
    void register_software_backend_factory();
}

#if defined(_WIN32)
namespace lithium::mica::direct2d {
    void register_direct2d_backend_factory();
}
#endif

#include <unordered_map>
#include <iostream>

namespace lithium::mica {

// ============================================================================
// Backend Detection
// ============================================================================

#if defined(_WIN32)
    #include <windows.h>

    BackendType get_preferred_backend() {
        // Windows: Direct2D is preferred, fallback to OpenGL
        // Check if Direct2D is available (Windows 7+)
        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

        // Direct2D is available on Windows 7+
        // For simplicity, we'll assume Direct2D is available
        return BackendType::Direct2D;
    }

#elif defined(__linux__) || defined(__ANDROID__)
    BackendType get_preferred_backend() {
        // Linux/Android: OpenGL is preferred
        return BackendType::OpenGL;
    }

#elif defined(__APPLE__)
    #include <TargetConditionals.h>

    BackendType get_preferred_backend() {
        #if TARGET_OS_IPHONE
            // iOS: Metal or OpenGL ES
            return BackendType::OpenGL;
        #else
            // macOS: Metal or OpenGL
            return BackendType::OpenGL;
        #endif
    }

#else
    BackendType get_preferred_backend() {
        // Unknown platform: fallback to software rendering
        return BackendType::Software;
    }
#endif

// ============================================================================
// Backend Registry
// ============================================================================

namespace {
    std::unordered_map<BackendType, BackendFactory> g_backend_factories;
}

void register_backend_factory(BackendType type, BackendFactory factory) {
    g_backend_factories[type] = std::move(factory);
    std::cout << "Mica: Registered backend factory for "
              << backend_type_name(type) << std::endl;
}

void register_available_backends() {
    // Register software backend (always available)
    software::register_software_backend_factory();

#if defined(_WIN32)
    // Register Direct2D backend (Windows only)
    direct2d::register_direct2d_backend_factory();
#endif
}

std::unique_ptr<IBackend> create_backend(BackendType type) {
    // Auto-detect if requested
    if (type == BackendType::Auto) {
        type = get_preferred_backend();
    }

    // Check if factory exists
    auto it = g_backend_factories.find(type);
    if (it != g_backend_factories.end()) {
        std::cout << "Mica: Creating " << backend_type_name(type) << " backend" << std::endl;
        return it->second();
    }

    // Fallback to software rendering
    std::cerr << "Mica: Failed to create " << backend_type_name(type)
              << " backend, falling back to software rendering" << std::endl;

    auto sw_it = g_backend_factories.find(BackendType::Software);
    if (sw_it != g_backend_factories.end()) {
        return sw_it->second();
    }

    std::cerr << "Mica: No available backends!" << std::endl;
    return nullptr;
}

std::unique_ptr<IBackend> initialize_default_backend() {
    BackendType preferred = get_preferred_backend();
    std::cout << "Mica: Preferred backend: " << backend_type_name(preferred) << std::endl;

    return create_backend(preferred);
}

} // namespace lithium::mica
