/**
 * X11 Window implementation for Linux
 */

#include "lithium/platform/window.hpp"
#include <cstring>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace lithium::platform {

// ============================================================================
// X11Window implementation
// ============================================================================

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

        m_title = config.title;
        m_width = config.width;
        m_height = config.height;

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
        poll_events();
    }

    void set_cursor_visible(bool visible) override {}

    void set_cursor_position(i32 x, i32 y) override {
        if (m_display && m_window) {
            XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, x, y);
        }
    }

    void* native_handle() const override {
        return reinterpret_cast<void*>(m_window);
    }

    String get_clipboard_text() const override { return ""_s; }
    void set_clipboard_text(const String& text) override {}

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
                    WindowResizeEvent resize_ev;
                    resize_ev.width = m_width;
                    resize_ev.height = m_height;
                    m_event_callback(resize_ev);
                }
                if (event.xconfigure.x != m_x ||
                    event.xconfigure.y != m_y) {
                    m_x = event.xconfigure.x;
                    m_y = event.xconfigure.y;
                    WindowMoveEvent move_ev;
                    move_ev.x = m_x;
                    move_ev.y = m_y;
                    m_event_callback(move_ev);
                }
                break;

            case FocusIn:
                m_focused = true;
                {
                    WindowFocusEvent focus_ev;
                    focus_ev.focused = true;
                    m_event_callback(focus_ev);
                }
                break;

            case FocusOut:
                m_focused = false;
                {
                    WindowFocusEvent focus_ev;
                    focus_ev.focused = false;
                    m_event_callback(focus_ev);
                }
                break;

            case KeyPress:
            case KeyRelease:
                {
                    KeyEvent key_ev;
                    key_ev.key = KeyCode::Unknown;
                    key_ev.scancode = event.xkey.keycode;
                    key_ev.pressed = (event.type == KeyPress);
                    key_ev.repeat = false;
                    key_ev.modifiers = KeyModifiers::NoMods;
                    m_event_callback(key_ev);
                }
                break;

            case ButtonPress:
            case ButtonRelease:
                {
                    MouseButtonEvent btn_ev;
                    btn_ev.button = static_cast<MouseButton>(event.xbutton.button - 1);
                    btn_ev.pressed = (event.type == ButtonPress);
                    btn_ev.modifiers = KeyModifiers::NoMods;
                    m_event_callback(btn_ev);
                }
                break;

            case MotionNotify:
                {
                    MouseMoveEvent move_ev;
                    move_ev.x = static_cast<f64>(event.xmotion.x);
                    move_ev.y = static_cast<f64>(event.xmotion.y);
                    m_event_callback(move_ev);
                }
                break;

            case EnterNotify:
                {
                    MouseEnterEvent enter_ev;
                    enter_ev.entered = true;
                    m_event_callback(enter_ev);
                }
                break;

            case LeaveNotify:
                {
                    MouseEnterEvent enter_ev;
                    enter_ev.entered = false;
                    m_event_callback(enter_ev);
                }
                break;

            case ClientMessage:
                if (static_cast<Atom>(event.xclient.data.l[0]) == m_wm_delete) {
                    m_should_close = true;
                    WindowCloseEvent close_ev;
                    m_event_callback(close_ev);
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

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<Window> Window::create(const WindowConfig& config) {
    return std::make_unique<X11Window>(config);
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
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        int screen = DefaultScreen(display);
        int width = DisplayWidth(display, screen);
        int height = DisplayHeight(display, screen);
        XCloseDisplay(display);
        return {width, height};
    }
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
