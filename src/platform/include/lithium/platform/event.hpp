#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>

namespace lithium::platform {

// ============================================================================
// Keyboard
// ============================================================================

enum class KeyCode : u16 {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Control keys
    Escape, Tab, CapsLock, Shift, Control, Alt, Super,
    LeftShift, RightShift, LeftControl, RightControl,
    LeftAlt, RightAlt, LeftSuper, RightSuper,
    Space, Enter, Backspace, Delete, Insert,
    Home, End, PageUp, PageDown,
    Left, Right, Up, Down,
    PrintScreen, ScrollLock, Pause, Menu,

    // Punctuation
    Apostrophe, Comma, Minus, Period, Slash,
    Semicolon, Equal, LeftBracket, Backslash, RightBracket,
    GraveAccent,

    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDecimal, NumpadDivide, NumpadMultiply,
    NumpadSubtract, NumpadAdd, NumpadEnter, NumpadEqual,
    NumLock,
};

enum class KeyModifiers : u8 {
    NoMods = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
    CapsLock = 1 << 4,
    NumLock = 1 << 5,
};

inline KeyModifiers operator|(KeyModifiers a, KeyModifiers b) {
    return static_cast<KeyModifiers>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline bool operator&(KeyModifiers a, KeyModifiers b) {
    return (static_cast<u8>(a) & static_cast<u8>(b)) != 0;
}

// ============================================================================
// Mouse
// ============================================================================

enum class MouseButton : u8 {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
};

// ============================================================================
// Events
// ============================================================================

// Window events
struct WindowCloseEvent {};

struct WindowResizeEvent {
    i32 width;
    i32 height;
};

struct WindowMoveEvent {
    i32 x;
    i32 y;
};

struct WindowFocusEvent {
    bool focused;
};

struct FramebufferResizeEvent {
    i32 width;
    i32 height;
};

struct ContentScaleEvent {
    f32 scale_x;
    f32 scale_y;
};

// Keyboard events
struct KeyEvent {
    KeyCode key;
    i32 scancode;
    bool pressed;
    bool repeat;
    KeyModifiers modifiers;
};

struct CharEvent {
    unicode::CodePoint codepoint;
};

// Mouse events
struct MouseButtonEvent {
    MouseButton button;
    bool pressed;
    KeyModifiers modifiers;
};

struct MouseMoveEvent {
    f64 x;
    f64 y;
};

struct MouseScrollEvent {
    f64 x_offset;
    f64 y_offset;
};

struct MouseEnterEvent {
    bool entered;
};

// File drop event
struct FileDropEvent {
    std::vector<String> paths;
};

// Event variant
using Event = std::variant<
    WindowCloseEvent,
    WindowResizeEvent,
    WindowMoveEvent,
    WindowFocusEvent,
    FramebufferResizeEvent,
    ContentScaleEvent,
    KeyEvent,
    CharEvent,
    MouseButtonEvent,
    MouseMoveEvent,
    MouseScrollEvent,
    MouseEnterEvent,
    FileDropEvent
>;

// Event type checking
template<typename T>
[[nodiscard]] bool is_event_type(const Event& event) {
    return std::holds_alternative<T>(event);
}

template<typename T>
[[nodiscard]] const T* get_event(const Event& event) {
    return std::get_if<T>(&event);
}

// ============================================================================
// Event Dispatcher
// ============================================================================

class EventDispatcher {
public:
    explicit EventDispatcher(const Event& event) : m_event(event) {}

    template<typename T, typename F>
    bool dispatch(F&& handler) {
        if (auto* e = std::get_if<T>(&m_event)) {
            m_handled = handler(*e);
            return m_handled;
        }
        return false;
    }

    [[nodiscard]] bool handled() const { return m_handled; }

private:
    const Event& m_event;
    bool m_handled{false};
};

} // namespace lithium::platform
