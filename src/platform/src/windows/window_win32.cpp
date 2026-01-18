/**
 * Win32 Window implementation for Windows
 */

#include "lithium/platform/window.hpp"
#include <windows.h>
#include <cstring>

namespace lithium::platform {

// ============================================================================
// Win32Window implementation
// ============================================================================

class Win32Window : public Window {
public:
    Win32Window(const WindowConfig& config) : m_config(config) {
        static bool class_registered = false;
        if (!class_registered) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"LithiumWindow";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassW(&wc);
            class_registered = true;
        }

        m_hwnd = CreateWindowExW(
            0,
            L"LithiumWindow",
            L"Lithium Browser",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            config.width, config.height,
            nullptr, nullptr,
            GetModuleHandle(nullptr),
            this
        );

        if (m_hwnd) {
            SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            m_title = config.title;
            m_width = config.width;
            m_height = config.height;

            if (config.visible) {
                ShowWindow(m_hwnd, SW_SHOW);
                m_visible = true;
            }
        }
    }

    ~Win32Window() override {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
    }

    String title() const override { return m_title; }

    void set_title(const String& title) override {
        m_title = title;
        if (m_hwnd) {
            // Convert UTF-8 to wide string
            int len = MultiByteToWideChar(CP_UTF8, 0, title.data(), -1, nullptr, 0);
            if (len > 0) {
                std::wstring wtitle(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, title.data(), -1, &wtitle[0], len);
                SetWindowTextW(m_hwnd, wtitle.c_str());
            }
        }
    }

    SizeI size() const override { return {m_width, m_height}; }

    void set_size(i32 width, i32 height) override {
        m_width = width;
        m_height = height;
        if (m_hwnd) {
            SetWindowPos(m_hwnd, nullptr, 0, 0, width, height,
                SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    PointI position() const override { return {m_x, m_y}; }

    void set_position(i32 x, i32 y) override {
        m_x = x;
        m_y = y;
        if (m_hwnd) {
            SetWindowPos(m_hwnd, nullptr, x, y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    SizeI framebuffer_size() const override { return size(); }
    f32 content_scale() const override { return 1.0f; }

    bool is_visible() const override { return m_visible; }

    void show() override {
        m_visible = true;
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
        }
    }

    void hide() override {
        m_visible = false;
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    bool is_minimized() const override { return m_minimized; }

    void minimize() override {
        m_minimized = true;
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_MINIMIZE);
        }
    }

    bool is_maximized() const override { return m_maximized; }
    void maximize() override {
        m_maximized = true;
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_MAXIMIZE);
        }
    }

    void restore() override {
        m_minimized = false;
        m_maximized = false;
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_RESTORE);
        }
    }

    bool is_focused() const override { return m_focused; }

    void focus() override {
        if (m_hwnd) {
            SetFocus(m_hwnd);
        }
    }

    bool is_fullscreen() const override { return m_fullscreen; }
    void set_fullscreen(bool fullscreen) override {
        m_fullscreen = fullscreen;
        // TODO: Implement fullscreen toggle
    }

    bool should_close() const override { return m_should_close; }

    void set_should_close(bool should_close) override {
        m_should_close = should_close;
    }

    void set_event_callback(EventCallback callback) override {
        m_event_callback = std::move(callback);
    }

    void poll_events() override {
        MSG msg;
        while (PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void wait_events() override {
        MSG msg;
        if (GetMessageW(&msg, m_hwnd, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        poll_events();
    }

    void wait_events_timeout(f64 timeout_seconds) override {
        poll_events();
    }

    void set_cursor_visible(bool visible) override {
        if (m_hwnd) {
            ShowCursor(visible ? TRUE : FALSE);
        }
    }

    void set_cursor_position(i32 x, i32 y) override {
        if (m_hwnd) {
            POINT pt = {x, y};
            ClientToScreen(m_hwnd, &pt);
            SetCursorPos(pt.x, pt.y);
        }
    }

    void* native_handle() const override {
        return reinterpret_cast<void*>(m_hwnd);
    }

    String get_clipboard_text() const override {
        if (!OpenClipboard(nullptr)) {
            return ""_s;
        }

        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData) {
            CloseClipboard();
            return ""_s;
        }

        wchar_t* text = static_cast<wchar_t*>(GlobalLock(hData));
        if (!text) {
            CloseClipboard();
            return ""_s;
        }

        // Convert wide string to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        String result;
        if (len > 0) {
            result = String(len, 0);
            WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], len, nullptr, nullptr);
        }

        GlobalUnlock(hData);
        CloseClipboard();

        return result;
    }

    void set_clipboard_text(const String& text) override {
        if (!OpenClipboard(nullptr)) {
            return;
        }

        // Convert UTF-8 to wide string
        int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), -1, nullptr, 0);
        if (len <= 0) {
            CloseClipboard();
            return;
        }

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
        if (!hMem) {
            CloseClipboard();
            return;
        }

        wchar_t* wtext = static_cast<wchar_t*>(GlobalLock(hMem));
        MultiByteToWideChar(CP_UTF8, 0, text.data(), -1, wtext, len);
        GlobalUnlock(hMem);

        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hMem);
        CloseClipboard();
    }

    void make_context_current() override {}
    void swap_buffers() override {}

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Win32Window* window = nullptr;
        if (msg == WM_CREATE) {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            window = static_cast<Win32Window*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (!window) {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        switch (msg) {
            case WM_CLOSE:
                window->m_should_close = true;
                if (window->m_event_callback) {
                    WindowCloseEvent ev;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_SIZE:
                window->m_width = LOWORD(lParam);
                window->m_height = HIWORD(lParam);
                if (window->m_event_callback) {
                    WindowResizeEvent ev;
                    ev.width = window->m_width;
                    ev.height = window->m_height;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_MOVE:
                window->m_x = LOWORD(lParam);
                window->m_y = HIWORD(lParam);
                if (window->m_event_callback) {
                    WindowMoveEvent ev;
                    ev.x = window->m_x;
                    ev.y = window->m_y;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_SETFOCUS:
                window->m_focused = true;
                if (window->m_event_callback) {
                    WindowFocusEvent ev;
                    ev.focused = true;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_KILLFOCUS:
                window->m_focused = false;
                if (window->m_event_callback) {
                    WindowFocusEvent ev;
                    ev.focused = false;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_KEYDOWN:
            case WM_KEYUP:
                if (window->m_event_callback) {
                    KeyEvent ev;
                    ev.key = static_cast<KeyCode>(wParam);
                    ev.scancode = static_cast<i32>(lParam & 0xFF);
                    ev.pressed = (msg == WM_KEYDOWN);
                    ev.repeat = (lParam & 0x40000000) != 0;
                    ev.modifiers = KeyModifiers::NoMods;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                if (window->m_event_callback) {
                    MouseButtonEvent ev;
                    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
                        ev.button = MouseButton::Left;
                    } else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) {
                        ev.button = MouseButton::Right;
                    } else {
                        ev.button = MouseButton::Middle;
                    }
                    ev.pressed = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN);
                    ev.modifiers = KeyModifiers::NoMods;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_MOUSEMOVE:
                if (window->m_event_callback) {
                    MouseMoveEvent ev;
                    ev.x = static_cast<f64>(LOWORD(lParam));
                    ev.y = static_cast<f64>(HIWORD(lParam));
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_MOUSELEAVE:
                if (window->m_event_callback) {
                    MouseEnterEvent ev;
                    ev.entered = false;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_MOUSEHOVER:
                if (window->m_event_callback) {
                    MouseEnterEvent ev;
                    ev.entered = true;
                    window->m_event_callback(ev);
                }
                return 0;

            case WM_SYSCOMMAND:
                if (wParam == SC_MINIMIZE) {
                    window->m_minimized = true;
                } else if (wParam == SC_RESTORE) {
                    window->m_minimized = false;
                    window->m_maximized = false;
                } else if (wParam == SC_MAXIMIZE) {
                    window->m_maximized = true;
                }
                break;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    WindowConfig m_config;
    HWND m_hwnd{nullptr};

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
    return std::make_unique<Win32Window>(config);
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
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        int width = GetDeviceCaps(hdc, HORZRES);
        int height = GetDeviceCaps(hdc, VERTRES);
        ReleaseDC(nullptr, hdc);
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
