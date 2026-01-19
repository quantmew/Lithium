#pragma once

#include "lithium/mica/types.hpp"
#include "lithium/mica/backend.hpp"
#include "lithium/core/memory.hpp"
#include <memory>

namespace lithium::mica {

// Forward declarations
class Painter;
class Texture;
class Buffer;
class Shader;
class RenderTarget;
class SwapChain;

// ============================================================================
// Graphics Context
// ============================================================================

/// Graphics rendering context
/// Represents the rendering state and resources for a window or surface
class Context {
public:
    virtual ~Context() = default;

    /// Get the backend that created this context
    [[nodiscard]] virtual IBackend* backend() noexcept = 0;

    /// Get the native window handle
    [[nodiscard]] virtual NativeWindowHandle native_window() const noexcept = 0;

    /// Get the swap chain
    [[nodiscard]] virtual SwapChain* swap_chain() noexcept = 0;

    /// Create a painter for this context
    [[nodiscard]] virtual std::unique_ptr<Painter> create_painter() = 0;

    /// Get current render target
    [[nodiscard]] virtual RenderTarget* current_render_target() noexcept = 0;

    /// Set current render target
    virtual void set_render_target(RenderTarget* target) = 0;

    /// Resize the swap chain
    virtual bool resize(i32 width, i32 height) = 0;

    /// Get size of the context
    [[nodiscard]] virtual Size size() const noexcept = 0;

    /// Begin frame (clear buffers, etc.)
    virtual void begin_frame() = 0;

    /// End frame (present to screen)
    virtual void end_frame() = 0;

    /// Present the rendered image
    virtual void present() = 0;

    /// Flush pending commands
    virtual void flush() = 0;

    /// Get DPI scaling factor
    [[nodiscard]] virtual f32 dpi_scale() const noexcept = 0;

    /// Check if context is valid
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;

protected:
    Context() = default;
};

// ============================================================================
// Swap Chain
// ============================================================================

/// Swap chain for double/triple buffering
class SwapChain {
public:
    virtual ~SwapChain() = default;

    /// Get swap chain size
    [[nodiscard]] virtual Size size() const noexcept = 0;

    /// Resize swap chain
    virtual bool resize(i32 width, i32 height) = 0;

    /// Get current back buffer index
    [[nodiscard]] virtual i32 current_buffer() const noexcept = 0;

    /// Get buffer count
    [[nodiscard]] virtual i32 buffer_count() const noexcept = 0;

    /// Check if vsync is enabled
    [[nodiscard]] virtual bool vsync() const noexcept = 0;

    /// Set vsync mode
    virtual void set_vsync(bool enabled) = 0;

protected:
    SwapChain() = default;
};

} // namespace lithium::mica
