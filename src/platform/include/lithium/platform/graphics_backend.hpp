#pragma once

#include "lithium/core/types.hpp"
#include "graphics_config.hpp"
#include <vector>
#include <string>

namespace lithium::platform {

// Forward declarations
class Window;
class GraphicsContext;

// ============================================================================
// Backend Error Types
// ============================================================================

/**
 * @brief Graphics backend initialization errors
 */
enum class BackendError {
    None,
    InitializationFailed,
    NotSupported,
    OutOfMemory,
    InvalidConfig,
    DeviceLost,
    CompilationFailed,
    LinkingFailed,
    Unknown
};

/**
 * @brief Convert backend error to string
 */
[[nodiscard]] constexpr const char* to_string(BackendError error) {
    switch (error) {
        case BackendError::None: return "None";
        case BackendError::InitializationFailed: return "InitializationFailed";
        case BackendError::NotSupported: return "NotSupported";
        case BackendError::OutOfMemory: return "OutOfMemory";
        case BackendError::InvalidConfig: return "InvalidConfig";
        case BackendError::DeviceLost: return "DeviceLost";
        case BackendError::CompilationFailed: return "CompilationFailed";
        case BackendError::LinkingFailed: return "LinkingFailed";
        case BackendError::Unknown: return "Unknown";
    }
    return "Unknown";
}

// ============================================================================
// Graphics Capabilities
// ============================================================================

/**
 * @brief Graphics backend capabilities
 */
struct GraphicsCapabilities {
    std::string backend_name;           // Human-readable backend name
    std::string renderer_name;          // GPU/driver name
    std::string vendor_name;            // GPU vendor name
    std::string version_string;         // Driver/version string

    bool hardware_accelerated;          // True if using GPU
    bool supports_vsync;                // VSync support
    bool supports_msaa;                 // Multisample anti-aliasing
    bool supports_shaders;              // Shader support
    bool supports_geometry_shaders;     // Geometry shader support
    bool supports_tessellation;         // Tessellation support
    bool supports_compute;              // Compute shader support

    i32 max_texture_size;               // Maximum texture dimension
    i32 max_texture_units;              // Maximum simultaneous textures
    i32 max_color_attachments;          // Maximum render targets
    i32 max_viewport_width;             // Maximum viewport width
    i32 max_viewport_height;            // Maximum viewport height

    i32 max_msaa_samples;               // Maximum MSAA samples

    f32 max_anisotropy;                 // Maximum anisotropic filtering

    // Limits
    i32 max_vertex_attributes;          // Maximum vertex shader attributes
    i32 max_vertex_uniform_components;  // Maximum vertex shader uniforms
    i32 max_fragment_uniform_components;// Maximum fragment shader uniforms

    // Features
    bool supports_srgb;                 // sRGB framebuffers
    bool supports_float_textures;       // Floating point textures
    bool supports_depth_texture;        // Depth textures
    bool supports_cube_maps;            // Cubemap textures
    bool supports_3d_textures;          // 3D textures
    bool supports_array_textures;       // Texture arrays
    bool supports_instancing;           // Instanced rendering
    bool supports_multi_draw_indirect;  // Multi-draw indirect

    GraphicsCapabilities()
        : backend_name("Unknown")
        , renderer_name("Unknown")
        , vendor_name("Unknown")
        , version_string("0.0.0")
        , hardware_accelerated(false)
        , supports_vsync(false)
        , supports_msaa(false)
        , supports_shaders(false)
        , supports_geometry_shaders(false)
        , supports_tessellation(false)
        , supports_compute(false)
        , max_texture_size(0)
        , max_texture_units(0)
        , max_color_attachments(0)
        , max_viewport_width(0)
        , max_viewport_height(0)
        , max_msaa_samples(0)
        , max_anisotropy(0.0f)
        , max_vertex_attributes(0)
        , max_vertex_uniform_components(0)
        , max_fragment_uniform_components(0)
        , supports_srgb(false)
        , supports_float_textures(false)
        , supports_depth_texture(false)
        , supports_cube_maps(false)
        , supports_3d_textures(false)
        , supports_array_textures(false)
        , supports_instancing(false)
        , supports_multi_draw_indirect(false)
    {}
};

// ============================================================================
// Backend Result Type
// ============================================================================

/**
 * @brief Result type for backend creation operations
 */
template<typename T>
using BackendResult = Result<T, BackendError>;

// ============================================================================
// Backend Factory
// ============================================================================

/**
 * @brief Factory for creating graphics contexts with backend selection
 */
class GraphicsBackendFactory {
public:
    /**
     * @brief Create a graphics context with the specified configuration
     * @param window The window to render to
     * @param config Graphics configuration
     * @return Result containing the context or an error
     */
    [[nodiscard]] static BackendResult<std::unique_ptr<GraphicsContext>>
    create(Window* window, const GraphicsConfig& config = {});

    /**
     * @brief Get list of available backend types on this platform
     * @return Vector of available backend types
     */
    [[nodiscard]] static std::vector<GraphicsConfig::BackendType> available_backends();

    /**
     * @brief Query capabilities of a specific backend type
     * @param backend The backend type to query
     * @return Result containing capabilities or an error
     */
    [[nodiscard]] static BackendResult<GraphicsCapabilities>
    query_capabilities(GraphicsConfig::BackendType backend);

    /**
     * @brief Get the default backend type for the current platform
     * @return Default backend type
     */
    [[nodiscard]] static GraphicsConfig::BackendType default_backend();

private:
    // Helper to try creating a specific backend
    [[nodiscard]] static BackendResult<std::unique_ptr<GraphicsContext>>
    try_create_backend(
        Window* window,
        const GraphicsConfig& config,
        GraphicsConfig::BackendType backend);

    // Platform-specific creation functions
    [[nodiscard]] static BackendResult<std::unique_ptr<GraphicsContext>>
    create_opengl(Window* window, const GraphicsConfig& config);

    [[nodiscard]] static BackendResult<std::unique_ptr<GraphicsContext>>
    create_direct2d(Window* window, const GraphicsConfig& config);

    [[nodiscard]] static BackendResult<std::unique_ptr<GraphicsContext>>
    create_software(Window* window);

    // Helper to check if backend is available
    [[nodiscard]] static bool is_opengl_available();
    [[nodiscard]] static bool is_direct2d_available();
};

} // namespace lithium::platform
