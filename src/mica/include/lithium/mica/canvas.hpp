#pragma once

#include "lithium/mica/types.hpp"
#include "lithium/mica/painter.hpp"
#include "lithium/core/memory.hpp"
#include <memory>

namespace lithium::mica {

// Forward declarations
class Context;
class Texture;

// ============================================================================
// Canvas
// ============================================================================

/// Canvas represents a drawing surface
/// Can be either a window surface or an off-screen buffer
class Canvas {
public:
    virtual ~Canvas() = default;

    /// Get canvas size
    [[nodiscard]] virtual Size size() const noexcept = 0;

    /// Resize canvas
    virtual bool resize(i32 width, i32 height) = 0;

    /// Get the painter for this canvas
    [[nodiscard]] virtual Painter* painter() noexcept = 0;

    /// Get the context
    [[nodiscard]] virtual Context* context() noexcept = 0;

    /// Get the texture (for off-screen canvases)
    [[nodiscard]] virtual Texture* texture() noexcept = 0;

    /// Check if this is an off-screen canvas
    [[nodiscard]] virtual bool is_offscreen() const noexcept = 0;

    /// Flush pending drawing commands
    virtual void flush() = 0;

    /// Begin drawing a frame
    virtual void begin_frame() = 0;

    /// End drawing a frame
    virtual void end_frame() = 0;

    /// Present the frame (for window canvases)
    virtual void present() = 0;

protected:
    Canvas() = default;
};

// ============================================================================
// Canvas Factory
// ============================================================================>

/// Create a window canvas (for rendering to a window)
[[nodiscard]] std::unique_ptr<Canvas> create_window_canvas(
    Context& context,
    NativeWindowHandle window);

/// Create an off-screen canvas
[[nodiscard]] std::unique_ptr<Canvas> create_offscreen_canvas(
    Context& context,
    i32 width,
    i32 height,
    ImageFormat format = ImageFormat::RGBA8);

/// Create an off-screen canvas from existing texture
[[nodiscard]] std::unique_ptr<Canvas> create_offscreen_canvas_from_texture(
    Context& context,
    Texture* texture);

} // namespace lithium::mica
