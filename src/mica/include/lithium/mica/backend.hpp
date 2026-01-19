#pragma once

#include "lithium/mica/types.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <vector>

namespace lithium::mica {

// Forward declarations
class Context;
class Texture;
class Buffer;
class Shader;
class RenderTarget;

// ============================================================================
// Backend Type
// ============================================================================

enum class BackendType {
    Auto,         ///< Auto-detect best backend
    OpenGL,       ///< OpenGL 3.3+ / ES 3.0+
    Direct2D,     ///< Direct2D (Windows only)
    Software,     ///< CPU software rendering
    Metal,        ///< Metal (macOS/iOS only) - Future
    Vulkan        ///< Vulkan - Future
};

/// Get human-readable name for backend type
[[nodiscard]] inline const char* backend_type_name(BackendType type) {
    switch (type) {
        case BackendType::Auto: return "Auto";
        case BackendType::OpenGL: return "OpenGL";
        case BackendType::Direct2D: return "Direct2D";
        case BackendType::Software: return "Software";
        case BackendType::Metal: return "Metal";
        case BackendType::Vulkan: return "Vulkan";
        default: return "Unknown";
    }
}

/// Get the best available backend for the current platform
[[nodiscard]] BackendType get_preferred_backend();

// ============================================================================
// Backend Capabilities
// ============================================================================

struct BackendCapabilities {
    bool supports_multisampling{false};
    bool supports_shaders{false};
    bool supports_compute{false};
    bool supports_framebuffer_srgb{false};
    i32 max_texture_size{0};
    i32 max_texture_units{0};
    i32 max_render_targets{1};
    i32 max_vertex_attributes{0};
    i32 max_uniform_buffer_bindings{0};
    f32 max_anisotropy{1.0f};

    /// Sample counts supported for MSAA
    std::vector<i32> sample_counts;
};

// ============================================================================
// Graphics Context Interface
// ============================================================================

/// Platform-specific window handle
struct NativeWindowHandle {
    void* handle{nullptr};

    // Platform-specific fields
#if defined(_WIN32)
    void* hwnd{nullptr};      // HWND
    void* hdc{nullptr};       // HDC
#elif defined(__linux__)
    u32 window{0};            // Window (X11)
    u32 display{0};           // Display* (X11)
#elif defined(__APPLE__)
    void* ns_window{nullptr}; // NSWindow*
    void* ns_view{nullptr};   // NSView*
#endif
};

/// Swap chain configuration
struct SwapChainConfig {
    i32 width{0};
    i32 height{0};
    i32 buffer_count{2};           ///< Double-buffering by default
    i32 sample_count{1};           ///< MSAA samples (1 = disabled)
    bool vsync{true};
    bool srgb{false};              ///< sRGB frame buffer
};

/// Render target description
struct RenderTargetDesc {
    i32 width{0};
    i32 height{0};
    ImageFormat format{ImageFormat::RGBA8};
    i32 sample_count{1};
    bool has_depth_buffer{false};
    bool has_stencil_buffer{false};
};

// ============================================================================
// Backend Interface - Abstract Base Class
// ============================================================================

/// Abstract backend interface
/// All rendering backends (Direct2D, OpenGL, etc.) must implement this interface
class IBackend {
public:
    virtual ~IBackend() = default;

    /// Get backend type
    [[nodiscard]] virtual BackendType type() const noexcept = 0;

    /// Get backend capabilities
    [[nodiscard]] virtual const BackendCapabilities& capabilities() const noexcept = 0;

    /// Create a graphics context for rendering to a window
    [[nodiscard]] virtual std::unique_ptr<Context> create_context(
        NativeWindowHandle window,
        const SwapChainConfig& config) = 0;

    /// Create an offscreen render target
    [[nodiscard]] virtual std::unique_ptr<RenderTarget> create_render_target(
        const RenderTargetDesc& desc) = 0;

    /// Create a texture from raw pixel data
    [[nodiscard]] virtual std::unique_ptr<Texture> create_texture(
        i32 width,
        i32 height,
        ImageFormat format,
        const void* data,
        i32 stride = 0) = 0;

    /// Create a vertex/index/uniform buffer
    [[nodiscard]] virtual std::unique_ptr<Buffer> create_buffer(
        BufferType type,
        BufferUsage usage,
        usize size,
        const void* data = nullptr) = 0;

    /// Create a shader program (for programmable pipelines)
    [[nodiscard]] virtual std::unique_ptr<Shader> create_shader(
        const String& vertex_source,
        const String& fragment_source) = 0;

    /// Flush pending commands
    virtual void flush() = 0;

    /// Finish all pending commands (blocking)
    virtual void finish() = 0;

protected:
    IBackend() = default;
};

// ============================================================================
// Backend Factory
// ============================================================================

/// Factory function to create backends
using BackendFactory = std::function<std::unique_ptr<IBackend>()>;

/// Register a backend factory for a specific type
void register_backend_factory(BackendType type, BackendFactory factory);

/// Register all available backend factories (call before creating backend)
void register_available_backends();

/// Create a backend instance
[[nodiscard]] std::unique_ptr<IBackend> create_backend(BackendType type);

/// Initialize the default backend for the current platform
[[nodiscard]] std::unique_ptr<IBackend> initialize_default_backend();

} // namespace lithium::mica
