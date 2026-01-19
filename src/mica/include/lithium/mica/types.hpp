#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <cstdint>
#include <memory>
#include <cmath>

namespace lithium::mica {

// ============================================================================
// Basic Types
// ============================================================================

using u8 = lithium::u8;
using u16 = lithium::u16;
using u32 = lithium::u32;
using u64 = lithium::u64;
using i8 = lithium::i8;
using i16 = lithium::i16;
using i32 = lithium::i32;
using i64 = lithium::i64;
using f32 = lithium::f32;
using f64 = lithium::f64;

using String = lithium::String;
using Vec2 = lithium::PointF;
using Rect = lithium::RectF;
using Size = lithium::SizeF;

// ============================================================================
// Vector and Matrix Types
// ============================================================================

template<typename T>
struct Vec3T {
    T x{}, y{}, z{};

    constexpr Vec3T() = default;
    constexpr Vec3T(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
};

using Vec3 = Vec3T<f32>;

template<typename T>
struct Vec4T {
    T x{}, y{}, z{}, w{};

    constexpr Vec4T() = default;
    constexpr Vec4T(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
};

using Vec4 = Vec4T<f32>;

template<typename T>
struct Mat3T {
    T m[3][3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1}
    };

    constexpr Mat3T() = default;

    [[nodiscard]] static constexpr Mat3T identity() {
        return Mat3T{};
    }

    [[nodiscard]] static constexpr Mat3T translation(T x, T y) {
        Mat3T result;
        result.m[2][0] = x;
        result.m[2][1] = y;
        return result;
    }

    [[nodiscard]] static constexpr Mat3T scale(T x, T y) {
        Mat3T result;
        result.m[0][0] = x;
        result.m[1][1] = y;
        return result;
    }

    [[nodiscard]] static constexpr Mat3T rotation(T angle) {
        Mat3T result;
        T c = std::cos(angle);
        T s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][1] = -s;
        result.m[1][0] = s;
        result.m[1][1] = c;
        return result;
    }

    // Multiply by Vec3 (for 2D transforms with homogeneous coordinates)
    template<typename V>
    [[nodiscard]] constexpr Vec3T<V> operator*(const Vec3T<V>& v) const {
        return Vec3T<V>{
            m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
        };
    }

    // Multiply by another Mat3 (for concatenating transforms)
    [[nodiscard]] constexpr Mat3T operator*(const Mat3T& other) const {
        Mat3T result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = m[0][j] * other.m[i][0] +
                                 m[1][j] * other.m[i][1] +
                                 m[2][j] * other.m[i][2];
            }
        }
        return result;
    }

    constexpr bool operator==(const Mat3T&) const = default;
};

using Mat3 = Mat3T<f32>;

template<typename T>
struct Mat4T {
    T m[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1}
    };

    constexpr Mat4T() = default;
};

using Mat4 = Mat4T<f32>;

// ============================================================================
// Color
// ============================================================================

class Color {
public:
    f32 r{0.0f}, g{0.0f}, b{0.0f}, a{1.0f};

    constexpr Color() = default;
    constexpr Color(f32 r, f32 g, f32 b, f32 a = 1.0f)
        : r(r), g(g), b(b), a(a) {}

    /// Create from RGBA values (0-255)
    static constexpr Color from_rgba(u8 red, u8 green, u8 blue, u8 alpha = 255) {
        return Color{
            static_cast<f32>(red) / 255.0f,
            static_cast<f32>(green) / 255.0f,
            static_cast<f32>(blue) / 255.0f,
            static_cast<f32>(alpha) / 255.0f
        };
    }

    /// Create from RGBA packed integer (0xAABBGGRR)
    static constexpr Color from_u32(u32 rgba) {
        return Color::from_rgba(
            static_cast<u8>(rgba & 0xFF),
            static_cast<u8>((rgba >> 8) & 0xFF),
            static_cast<u8>((rgba >> 16) & 0xFF),
            static_cast<u8>((rgba >> 24) & 0xFF)
        );
    }

    /// Predefined colors
    static constexpr Color transparent() { return Color(0, 0, 0, 0); }
    static constexpr Color black() { return Color(0, 0, 0, 1); }
    static constexpr Color white() { return Color(1, 1, 1, 1); }
    static constexpr Color red() { return Color(1, 0, 0, 1); }
    static constexpr Color green() { return Color(0, 1, 0, 1); }
    static constexpr Color blue() { return Color(0, 0, 1, 1); }

    /// Convert to packed pixel (BGRA for Windows GDI BI_RGB format)
    /// Memory layout (little-endian): B, G, R, A
    /// Integer value: 0xAARRGGBB (where RR=Red, GG=Green, BB=Blue, AA=Alpha as bytes)
    [[nodiscard]] u32 to_u32() const {
        return (
            (static_cast<u8>(a * 255.0f) << 24) |  // Alpha (bits 24-31)
            (static_cast<u8>(r * 255.0f) << 16) |  // Red (bits 16-23)
            (static_cast<u8>(g * 255.0f) << 8) |   // Green (bits 8-15)
            static_cast<u8>(b * 255.0f)             // Blue (bits 0-7)
        );
    }

    /// Premultiply alpha
    [[nodiscard]] Color premultiplied() const {
        return Color(r * a, g * a, b * a, a);
    }

    constexpr bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    constexpr bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

// ============================================================================
// Blend Mode
// ============================================================================

enum class BlendMode {
    SourceOver,     ///< Source on top of destination
    SourceIn,        ///< Source inside destination
    SourceOut,       ///< Source outside destination
    SourceAtop,      ///< Source atop destination
    DestinationOver, ///< Destination on top of source
    DestinationIn,   ///< Destination inside source
    DestinationOut,  ///< Destination outside source
    DestinationAtop, ///< Destination atop source
    Lighter,         ///< Sum of both
    Copy,            ///< Source only
    Xor,             ///< Source xor destination
};

// ============================================================================
// Brush Types
// ============================================================================

enum class BrushType {
    Solid,      ///< Solid color
    Linear,     ///< Linear gradient
    Radial,     ///< Radial gradient
    Image,      ///< Image/texture
    Pattern     ///< Repeating pattern
};

// ============================================================================
// Gradient Stops
// ============================================================================

struct GradientStop {
    f32 offset;  ///< 0.0 to 1.0
    Color color;
};

// ============================================================================
// Line Cap and Join
// ============================================================================

enum class LineCap {
    Butt,   ///< Flat edge
    Round,  ///< Rounded edge
    Square  ///< Square edge
};

enum class LineJoin {
    Miter,  ///< Sharp corner
    Round,  ///< Rounded corner
    Bevel   ///< Beveled corner
};

// ============================================================================
// Image Format
// ============================================================================

enum class ImageFormat {
    Unknown,
    RGBA8,     ///< 8-bit RGBA
    RGB8,      ///< 8-bit RGB
    BGRA8,     ///< 8-bit BGRA (Windows default)
    BGR8,      ///< 8-bit BGR
    A8,        ///< 8-bit alpha only
    R8,        ///< 8-bit red/grayscale only
    RG8,       ///< 8-bit RG (grayscale + alpha)
    RGBA16F,   ///< 16-bit float per channel
    RGB16F,    ///< 16-bit float RGB
    RGBA32F,   ///< 32-bit float per channel
    D24S8      ///< 24-bit depth + 8-bit stencil
};

/// Get bytes per pixel for format
[[nodiscard]] constexpr i32 bytes_per_pixel(ImageFormat format) {
    switch (format) {
        case ImageFormat::RGBA8:
        case ImageFormat::BGRA8:
        case ImageFormat::RGB8:
        case ImageFormat::BGR8:
            return 4;
        case ImageFormat::A8:
        case ImageFormat::R8:
            return 1;
        case ImageFormat::RG8:
            return 2;
        case ImageFormat::RGBA16F:
            return 8;
        case ImageFormat::RGB16F:
            return 6;
        case ImageFormat::RGBA32F:
            return 16;
        case ImageFormat::D24S8:
            return 4;
        default:
            return 0;
    }
}

// ============================================================================
// Texture Options
// ============================================================================

enum class FilterMode {
    Nearest,   ///< No filtering
    Linear,    ///< Linear interpolation
    Trilinear  ///< Trilinear (with mipmaps)
};

enum class WrapMode {
    Repeat,    ///< Repeat texture
    Clamp,     ///< Clamp to edge
    Mirror     ///< Mirror texture
};

// ============================================================================
// Primitive Type
// ============================================================================

enum class PrimitiveType {
    Triangles,
    TriangleStrip,
    TriangleFan,
    Lines,
    LineStrip,
    Points
};

// ============================================================================
// Vertex Format
// ============================================================================

enum class VertexAttributeType {
    Float,
    Float2,
    Float3,
    Float4,
    UByte4,
    Short2,
    Short4
};

struct VertexAttribute {
    const char* name;
    VertexAttributeType type;
    i32 offset;
    bool normalized{false};
};

// ============================================================================
// Buffer Type
// ============================================================================

enum class BufferType {
    Vertex,
    Index,
    Uniform
};

enum class BufferUsage {
    Static,    ///< Set once, used many times
    Dynamic,   ///< Set occasionally, used many times
    Stream     ///< Set every frame
};

} // namespace lithium::mica
