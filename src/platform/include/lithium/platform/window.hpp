#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include "event.hpp"
#include <memory>
#include <functional>

namespace lithium::platform {

// ============================================================================
// Window Configuration
// ============================================================================

struct WindowConfig {
    String title{"Lithium Browser"};
    i32 width{800};
    i32 height{600};
    i32 x{-1};  // -1 = centered
    i32 y{-1};
    bool resizable{true};
    bool decorated{true};  // Has title bar
    bool visible{true};
    bool maximized{false};
    bool fullscreen{false};
};

// ============================================================================
// Window - Platform window abstraction
// ============================================================================

class Window {
public:
    using EventCallback = std::function<void(const Event&)>;

    virtual ~Window() = default;

    // Factory
    [[nodiscard]] static std::unique_ptr<Window> create(const WindowConfig& config = {});

    // Window properties
    [[nodiscard]] virtual String title() const = 0;
    virtual void set_title(const String& title) = 0;

    [[nodiscard]] virtual SizeI size() const = 0;
    virtual void set_size(i32 width, i32 height) = 0;

    [[nodiscard]] virtual PointI position() const = 0;
    virtual void set_position(i32 x, i32 y) = 0;

    [[nodiscard]] virtual SizeI framebuffer_size() const = 0;
    [[nodiscard]] virtual f32 content_scale() const = 0;

    // Window state
    [[nodiscard]] virtual bool is_visible() const = 0;
    virtual void show() = 0;
    virtual void hide() = 0;

    [[nodiscard]] virtual bool is_minimized() const = 0;
    virtual void minimize() = 0;

    [[nodiscard]] virtual bool is_maximized() const = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;

    [[nodiscard]] virtual bool is_focused() const = 0;
    virtual void focus() = 0;

    [[nodiscard]] virtual bool is_fullscreen() const = 0;
    virtual void set_fullscreen(bool fullscreen) = 0;

    // Should close flag
    [[nodiscard]] virtual bool should_close() const = 0;
    virtual void set_should_close(bool should_close) = 0;

    // Event handling
    virtual void set_event_callback(EventCallback callback) = 0;
    virtual void poll_events() = 0;
    virtual void wait_events() = 0;
    virtual void wait_events_timeout(f64 timeout_seconds) = 0;

    // Cursor
    virtual void set_cursor_visible(bool visible) = 0;
    virtual void set_cursor_position(i32 x, i32 y) = 0;

    // Native handle (platform-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;

    // Clipboard
    [[nodiscard]] virtual String get_clipboard_text() const = 0;
    virtual void set_clipboard_text(const String& text) = 0;

    // OpenGL context (if using OpenGL backend)
    virtual void make_context_current() = 0;
    virtual void swap_buffers() = 0;

protected:
    Window() = default;
};

// ============================================================================
// Platform initialization
// ============================================================================

namespace platform {

// Initialize platform subsystem
bool init();

// Shutdown platform subsystem
void shutdown();

// Get primary monitor size
[[nodiscard]] SizeI primary_monitor_size();

// Get available monitors
struct MonitorInfo {
    String name;
    SizeI size;
    PointI position;
    f32 content_scale;
    i32 refresh_rate;
};

[[nodiscard]] std::vector<MonitorInfo> get_monitors();

} // namespace platform

} // namespace lithium::platform
