/**
 * Platform Window implementation (stub with X11 basic support)
 */

#include "lithium/platform/window.hpp"
#include <cstring>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace lithium::platform {

// ============================================================================
// Stub Window implementation for Linux/X11
// ============================================================================

#ifdef __linux__

class X11Window : public Window {
public:
    X11Window(const WindowConfig& config) : m_config(config) {
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            return;
        }

        m_screen = DefaultScreen(m_display);
        m_window = XCreateSimpleWindow(
            m_display,
            RootWindow(m_display, m_screen),
            config.x >= 0 ? config.x : 100,
            config.y >= 0 ? config.y : 100,
            config.width,
            config.height,
            1,
            BlackPixel(m_display, m_screen),
            WhitePixel(m_display, m_screen)
        );

        XStoreName(m_display, m_window, config.title.data());

        // Select events
        XSelectInput(m_display, m_window,
            ExposureMask | KeyPressMask | KeyReleaseMask |
            ButtonPressMask | ButtonReleaseMask |
            PointerMotionMask | StructureNotifyMask |
            FocusChangeMask | EnterWindowMask | LeaveWindowMask);

        // WM_DELETE_WINDOW protocol
        m_wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(m_display, m_window, &m_wm_delete, 1);

        if (config.visible) {
            XMapWindow(m_display, m_window);
        }

        XFlush(m_display);
    }

    ~X11Window() override {
        if (m_display) {
            if (m_window) {
                XDestroyWindow(m_display, m_window);
            }
            XCloseDisplay(m_display);
        }
    }

    String title() const override { return m_title; }
    void set_title(const String& title) override {
        m_title = title;
        if (m_display && m_window) {
            XStoreName(m_display, m_window, title.data());
        }
    }

    SizeI size() const override { return {m_width, m_height}; }
    void set_size(i32 width, i32 height) override {
        m_width = width;
        m_height = height;
        if (m_display && m_window) {
            XResizeWindow(m_display, m_window, width, height);
        }
    }

    PointI position() const override { return {m_x, m_y}; }
    void set_position(i32 x, i32 y) override {
        m_x = x;
        m_y = y;
        if (m_display && m_window) {
            XMoveWindow(m_display, m_window, x, y);
        }
    }

    SizeI framebuffer_size() const override { return size(); }
    f32 content_scale() const override { return 1.0f; }

    bool is_visible() const override { return m_visible; }
    void show() override {
        m_visible = true;
        if (m_display && m_window) {
            XMapWindow(m_display, m_window);
        }
    }
    void hide() override {
        m_visible = false;
        if (m_display && m_window) {
            XUnmapWindow(m_display, m_window);
        }
    }

    bool is_minimized() const override { return m_minimized; }
    void minimize() override {
        m_minimized = true;
        if (m_display && m_window) {
            XIconifyWindow(m_display, m_window, m_screen);
        }
    }

    bool is_maximized() const override { return m_maximized; }
    void maximize() override { m_maximized = true; }
    void restore() override {
        m_minimized = false;
        m_maximized = false;
    }

    bool is_focused() const override { return m_focused; }
    void focus() override {
        if (m_display && m_window) {
            XRaiseWindow(m_display, m_window);
            XSetInputFocus(m_display, m_window, RevertToParent, CurrentTime);
        }
    }

    bool is_fullscreen() const override { return m_fullscreen; }
    void set_fullscreen(bool fullscreen) override { m_fullscreen = fullscreen; }

    bool should_close() const override { return m_should_close; }
    void set_should_close(bool should_close) override { m_should_close = should_close; }

    void set_event_callback(EventCallback callback) override {
        m_event_callback = std::move(callback);
    }

    void poll_events() override {
        if (!m_display) return;

        while (XPending(m_display) > 0) {
            XEvent event;
            XNextEvent(m_display, &event);
            process_event(event);
        }
    }

    void wait_events() override {
        if (!m_display) return;

        XEvent event;
        XNextEvent(m_display, &event);
        process_event(event);
        poll_events();
    }

    void wait_events_timeout(f64 timeout_seconds) override {
        // Simplified - just poll
        poll_events();
    }

    void set_cursor_visible(bool visible) override {
        // TODO: Implement cursor visibility
    }

    void set_cursor_position(i32 x, i32 y) override {
        if (m_display && m_window) {
            XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, x, y);
        }
    }

    void* native_handle() const override {
        return reinterpret_cast<void*>(m_window);
    }

    String get_clipboard_text() const override {
        // TODO: Implement clipboard
        return ""_s;
    }

    void set_clipboard_text(const String& text) override {
        // TODO: Implement clipboard
    }

    void make_context_current() override {}
    void swap_buffers() override {}

private:
    void process_event(const XEvent& event) {
        if (!m_event_callback) return;

        switch (event.type) {
            case ConfigureNotify:
                if (event.xconfigure.width != m_width ||
                    event.xconfigure.height != m_height) {
                    m_width = event.xconfigure.width;
                    m_height = event.xconfigure.height;
                    m_event_callback(WindowResizeEvent{m_width, m_height});
                }
                if (event.xconfigure.x != m_x ||
                    event.xconfigure.y != m_y) {
                    m_x = event.xconfigure.x;
                    m_y = event.xconfigure.y;
                    m_event_callback(WindowMoveEvent{m_x, m_y});
                }
                break;

            case FocusIn:
                m_focused = true;
                m_event_callback(WindowFocusEvent{true});
                break;

            case FocusOut:
                m_focused = false;
                m_event_callback(WindowFocusEvent{false});
                break;

            case KeyPress:
            case KeyRelease:
                m_event_callback(KeyEvent{
                    KeyCode::Unknown,
                    event.xkey.keycode,
                    event.type == KeyPress,
                    false,
                    KeyModifiers::None
                });
                break;

            case ButtonPress:
            case ButtonRelease:
                m_event_callback(MouseButtonEvent{
                    static_cast<MouseButton>(event.xbutton.button - 1),
                    event.type == ButtonPress,
                    KeyModifiers::None
                });
                break;

            case MotionNotify:
                m_event_callback(MouseMoveEvent{
                    static_cast<f64>(event.xmotion.x),
                    static_cast<f64>(event.xmotion.y)
                });
                break;

            case EnterNotify:
                m_event_callback(MouseEnterEvent{true});
                break;

            case LeaveNotify:
                m_event_callback(MouseEnterEvent{false});
                break;

            case ClientMessage:
                if (static_cast<Atom>(event.xclient.data.l[0]) == m_wm_delete) {
                    m_should_close = true;
                    m_event_callback(WindowCloseEvent{});
                }
                break;
        }
    }

    WindowConfig m_config;
    Display* m_display{nullptr};
    ::Window m_window{0};
    int m_screen{0};
    Atom m_wm_delete{0};

    String m_title;
    i32 m_width{800};
    i32 m_height{600};
    i32 m_x{0};
    i32 m_y{0};

    bool m_visible{true};
    bool m_minimized{false};
    bool m_maximized{false};
    bool m_focused{false};
    bool m_fullscreen{false};
    bool m_should_close{false};

    EventCallback m_event_callback;
};

#endif // __linux__

// ============================================================================
// Stub Window for other platforms
// ============================================================================

#ifndef __linux__

class StubWindow : public Window {
public:
    StubWindow(const WindowConfig& config) : m_config(config) {}

    String title() const override { return m_config.title; }
    void set_title(const String& title) override { m_config.title = title; }

    SizeI size() const override { return {m_config.width, m_config.height}; }
    void set_size(i32 width, i32 height) override {
        m_config.width = width;
        m_config.height = height;
    }

    PointI position() const override { return {m_config.x, m_config.y}; }
    void set_position(i32 x, i32 y) override {
        m_config.x = x;
        m_config.y = y;
    }

    SizeI framebuffer_size() const override { return size(); }
    f32 content_scale() const override { return 1.0f; }

    bool is_visible() const override { return m_config.visible; }
    void show() override { m_config.visible = true; }
    void hide() override { m_config.visible = false; }

    bool is_minimized() const override { return false; }
    void minimize() override {}

    bool is_maximized() const override { return m_config.maximized; }
    void maximize() override { m_config.maximized = true; }
    void restore() override { m_config.maximized = false; }

    bool is_focused() const override { return true; }
    void focus() override {}

    bool is_fullscreen() const override { return m_config.fullscreen; }
    void set_fullscreen(bool fullscreen) override { m_config.fullscreen = fullscreen; }

    bool should_close() const override { return m_should_close; }
    void set_should_close(bool should_close) override { m_should_close = should_close; }

    void set_event_callback(EventCallback callback) override {}
    void poll_events() override {}
    void wait_events() override {}
    void wait_events_timeout(f64 timeout_seconds) override {}

    void set_cursor_visible(bool visible) override {}
    void set_cursor_position(i32 x, i32 y) override {}

    void* native_handle() const override { return nullptr; }

    String get_clipboard_text() const override { return ""_s; }
    void set_clipboard_text(const String& text) override {}

    void make_context_current() override {}
    void swap_buffers() override {}

private:
    WindowConfig m_config;
    bool m_should_close{false};
};

#endif // !__linux__

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<Window> Window::create(const WindowConfig& config) {
#ifdef __linux__
    return std::make_unique<X11Window>(config);
#else
    return std::make_unique<StubWindow>(config);
#endif
}

// ============================================================================
// Platform functions
// ============================================================================

namespace platform {

bool init() {
    return true;
}

void shutdown() {
}

SizeI primary_monitor_size() {
#ifdef __linux__
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        int screen = DefaultScreen(display);
        int width = DisplayWidth(display, screen);
        int height = DisplayHeight(display, screen);
        XCloseDisplay(display);
        return {width, height};
    }
#endif
    return {1920, 1080};
}

std::vector<MonitorInfo> get_monitors() {
    std::vector<MonitorInfo> monitors;
    auto size = primary_monitor_size();
    monitors.push_back({
        "Primary"_s,
        size,
        {0, 0},
        1.0f,
        60
    });
    return monitors;
}

} // namespace platform

} // namespace lithium::platform
