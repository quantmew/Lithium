#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <variant>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <atomic>
#include <functional>

namespace lithium {

// ============================================================================
// Basic type aliases
// ============================================================================

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// ============================================================================
// Result type - For error handling without exceptions
// ============================================================================

template<typename E>
struct Error {
    E value;

    explicit Error(E e) : value(std::move(e)) {}
};

template<typename E>
Error<std::decay_t<E>> make_error(E&& e) {
    return Error<std::decay_t<E>>(std::forward<E>(e));
}

template<typename T, typename E>
class Result {
public:
    using ValueType = T;
    using ErrorType = E;

    // Constructors
    template<typename U = T>
    Result(U&& value) : m_data(std::forward<U>(value)) {}
    Result(Error<E> error) : m_data(std::move(error.value)) {}

    // Check state
    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(m_data);
    }

    [[nodiscard]] bool is_err() const noexcept {
        return std::holds_alternative<E>(m_data);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return is_ok();
    }

    // Access value
    [[nodiscard]] T& value() & {
        return std::get<T>(m_data);
    }

    [[nodiscard]] const T& value() const& {
        return std::get<T>(m_data);
    }

    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(m_data));
    }

    // Access error
    [[nodiscard]] E& error() & {
        return std::get<E>(m_data);
    }

    [[nodiscard]] const E& error() const& {
        return std::get<E>(m_data);
    }

    [[nodiscard]] E&& error() && {
        return std::get<E>(std::move(m_data));
    }

    // Value or default
    [[nodiscard]] T value_or(T default_value) const& {
        if (is_ok()) {
            return std::get<T>(m_data);
        }
        return default_value;
    }

    [[nodiscard]] T value_or(T default_value) && {
        if (is_ok()) {
            return std::get<T>(std::move(m_data));
        }
        return default_value;
    }

    // Map operations
    template<typename F>
    auto map(F&& f) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        if (is_ok()) {
            return std::invoke(std::forward<F>(f), value());
        }
        return make_error(error());
    }

    template<typename F>
    auto map_err(F&& f) const& -> Result<T, std::invoke_result_t<F, const E&>> {
        if (is_err()) {
            return make_error(std::invoke(std::forward<F>(f), error()));
        }
        return value();
    }

private:
    std::variant<T, E> m_data;
};

// Specialization for void value type
template<typename E>
class Result<void, E> {
public:
    using ValueType = void;
    using ErrorType = E;

    Result() : m_error(std::nullopt) {}
    Result(Error<E> error) : m_error(std::move(error.value)) {}

    [[nodiscard]] bool is_ok() const noexcept {
        return !m_error.has_value();
    }

    [[nodiscard]] bool is_err() const noexcept {
        return m_error.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] E& error() & {
        return *m_error;
    }

    [[nodiscard]] const E& error() const& {
        return *m_error;
    }

private:
    std::optional<E> m_error;
};

// ============================================================================
// RefPtr - Reference counted smart pointer (thread-safe)
// ============================================================================

template<typename T>
class RefPtr {
public:
    RefPtr() noexcept : m_ptr(nullptr) {}
    RefPtr(std::nullptr_t) noexcept : m_ptr(nullptr) {}

    explicit RefPtr(T* ptr) : m_ptr(ptr) {
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    RefPtr(const RefPtr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    RefPtr(RefPtr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefPtr(const RefPtr<U>& other) noexcept : m_ptr(other.get()) {
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    ~RefPtr() {
        if (m_ptr) {
            m_ptr->release();
        }
    }

    RefPtr& operator=(const RefPtr& other) noexcept {
        if (this != &other) {
            if (m_ptr) {
                m_ptr->release();
            }
            m_ptr = other.m_ptr;
            if (m_ptr) {
                m_ptr->add_ref();
            }
        }
        return *this;
    }

    RefPtr& operator=(RefPtr&& other) noexcept {
        if (this != &other) {
            if (m_ptr) {
                m_ptr->release();
            }
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    RefPtr& operator=(std::nullptr_t) noexcept {
        if (m_ptr) {
            m_ptr->release();
            m_ptr = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return m_ptr; }
    [[nodiscard]] T* operator->() const noexcept { return m_ptr; }
    [[nodiscard]] T& operator*() const noexcept { return *m_ptr; }

    [[nodiscard]] explicit operator bool() const noexcept { return m_ptr != nullptr; }

    void reset() noexcept {
        if (m_ptr) {
            m_ptr->release();
            m_ptr = nullptr;
        }
    }

    [[nodiscard]] bool operator==(const RefPtr& other) const noexcept {
        return m_ptr == other.m_ptr;
    }

    [[nodiscard]] bool operator!=(const RefPtr& other) const noexcept {
        return m_ptr != other.m_ptr;
    }

    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept {
        return m_ptr != nullptr;
    }

private:
    T* m_ptr;
};

template<typename T, typename... Args>
RefPtr<T> make_ref(Args&&... args) {
    return RefPtr<T>(new T(std::forward<Args>(args)...));
}

// ============================================================================
// RefCounted - Base class for reference counted objects
// ============================================================================

class RefCounted {
public:
    RefCounted() : m_ref_count(0) {}
    virtual ~RefCounted() = default;

    RefCounted(const RefCounted&) : m_ref_count(0) {}
    RefCounted& operator=(const RefCounted&) { return *this; }

    void add_ref() const noexcept {
        m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const noexcept {
        if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    [[nodiscard]] u32 ref_count() const noexcept {
        return m_ref_count.load(std::memory_order_relaxed);
    }

private:
    mutable std::atomic<u32> m_ref_count;
};

// ============================================================================
// NonNull - Wrapper to express non-null pointers
// ============================================================================

template<typename T>
class NonNull {
public:
    NonNull(T* ptr) : m_ptr(ptr) {
        if (!m_ptr) {
            std::terminate();
        }
    }

    [[nodiscard]] T* get() const noexcept { return m_ptr; }
    [[nodiscard]] T* operator->() const noexcept { return m_ptr; }
    [[nodiscard]] T& operator*() const noexcept { return *m_ptr; }

private:
    T* m_ptr;
};

// ============================================================================
// Geometry types
// ============================================================================

template<typename T>
struct Point {
    T x{};
    T y{};

    constexpr Point() = default;
    constexpr Point(T x_, T y_) : x(x_), y(y_) {}

    constexpr bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    constexpr bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    constexpr Point operator+(const Point& other) const {
        return {x + other.x, y + other.y};
    }

    constexpr Point operator-(const Point& other) const {
        return {x - other.x, y - other.y};
    }
};

template<typename T>
struct Size {
    T width{};
    T height{};

    constexpr Size() = default;
    constexpr Size(T w, T h) : width(w), height(h) {}

    constexpr bool operator==(const Size& other) const {
        return width == other.width && height == other.height;
    }

    constexpr bool operator!=(const Size& other) const {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool is_empty() const {
        return width <= T{} || height <= T{};
    }
};

template<typename T>
struct Rect {
    T x{};
    T y{};
    T width{};
    T height{};

    constexpr Rect() = default;
    constexpr Rect(T x_, T y_, T w, T h) : x(x_), y(y_), width(w), height(h) {}
    constexpr Rect(Point<T> origin, Size<T> size)
        : x(origin.x), y(origin.y), width(size.width), height(size.height) {}

    [[nodiscard]] constexpr Point<T> origin() const { return {x, y}; }
    [[nodiscard]] constexpr Size<T> size() const { return {width, height}; }

    [[nodiscard]] constexpr T left() const { return x; }
    [[nodiscard]] constexpr T top() const { return y; }
    [[nodiscard]] constexpr T right() const { return x + width; }
    [[nodiscard]] constexpr T bottom() const { return y + height; }

    [[nodiscard]] constexpr bool is_empty() const {
        return width <= T{} || height <= T{};
    }

    [[nodiscard]] constexpr bool contains(Point<T> point) const {
        return point.x >= x && point.x < right() &&
               point.y >= y && point.y < bottom();
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const {
        return x < other.right() && right() > other.x &&
               y < other.bottom() && bottom() > other.y;
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& other) const {
        T new_x = std::max(x, other.x);
        T new_y = std::max(y, other.y);
        T new_right = std::min(right(), other.right());
        T new_bottom = std::min(bottom(), other.bottom());

        if (new_right <= new_x || new_bottom <= new_y) {
            return {};
        }

        return {new_x, new_y, new_right - new_x, new_bottom - new_y};
    }

    constexpr bool operator==(const Rect& other) const {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }

    constexpr bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};

// Common type aliases
using PointI = Point<i32>;
using PointF = Point<f32>;
using SizeI = Size<i32>;
using SizeF = Size<f32>;
using RectI = Rect<i32>;
using RectF = Rect<f32>;

// ============================================================================
// Color
// ============================================================================

struct Color {
    u8 r{0};
    u8 g{0};
    u8 b{0};
    u8 a{255};

    constexpr Color() = default;
    constexpr Color(u8 r_, u8 g_, u8 b_, u8 a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color from_rgb(u32 rgb) {
        return Color(
            static_cast<u8>((rgb >> 16) & 0xFF),
            static_cast<u8>((rgb >> 8) & 0xFF),
            static_cast<u8>(rgb & 0xFF)
        );
    }

    static constexpr Color from_rgba(u32 rgba) {
        return Color(
            static_cast<u8>((rgba >> 24) & 0xFF),
            static_cast<u8>((rgba >> 16) & 0xFF),
            static_cast<u8>((rgba >> 8) & 0xFF),
            static_cast<u8>(rgba & 0xFF)
        );
    }

    [[nodiscard]] constexpr u32 to_rgb() const {
        return (static_cast<u32>(r) << 16) |
               (static_cast<u32>(g) << 8) |
               static_cast<u32>(b);
    }

    [[nodiscard]] constexpr u32 to_rgba() const {
        return (static_cast<u32>(r) << 24) |
               (static_cast<u32>(g) << 16) |
               (static_cast<u32>(b) << 8) |
               static_cast<u32>(a);
    }

    constexpr bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    constexpr bool operator!=(const Color& other) const {
        return !(*this == other);
    }

    // Common colors
    static constexpr Color black() { return {0, 0, 0}; }
    static constexpr Color white() { return {255, 255, 255}; }
    static constexpr Color red() { return {255, 0, 0}; }
    static constexpr Color green() { return {0, 255, 0}; }
    static constexpr Color blue() { return {0, 0, 255}; }
    static constexpr Color transparent() { return {0, 0, 0, 0}; }
};

} // namespace lithium
