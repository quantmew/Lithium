#pragma once

#include "lithium/core/types.hpp"

namespace lithium::platform {

// ============================================================================
// Graphics Configuration
// ============================================================================

/**
 * @brief Configuration for graphics backend creation
 */
struct GraphicsConfig {
    /**
     * @brief Available backend types
     */
    enum class BackendType {
        Auto,       // Platform default (Direct2D on Windows, OpenGL on Linux/macOS)
        OpenGL,     // Request OpenGL backend
        Direct2D,   // Request Direct2D backend (Windows only)
        Software    // Force software rendering
    };

    /**
     * @brief Preferred backend type
     */
    BackendType preferred_backend = BackendType::Auto;

    /**
     * @brief Enable vertical synchronization
     */
    bool enable_vsync = true;

    /**
     * @brief MSAA samples (0 = disabled, 2/4/8 = sample count)
     */
    i32 msaa_samples = 0;

    /**
     * @brief Allow fallback to software rendering on backend failure
     */
    bool allow_fallback = true;

    /**
     * @brief Enable hardware acceleration
     */
    bool enable_hardware_acceleration = true;

    /**
     * @brief Minimum OpenGL version (for OpenGL backend)
     * Major version in high 16 bits, minor in low 16 bits
     */
    u32 min_opengl_version = 0x00030003; // OpenGL 3.3

    /**
     * @brief Enable debug context (validation layers, error reporting)
     */
    bool enable_debug = false;
};

} // namespace lithium::platform
