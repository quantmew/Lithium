#pragma once

#include "lithium/mica/types.hpp"
#include "lithium/mica/resource.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <vector>

// Forward declarations for beryl types
namespace lithium::beryl {
class FontDescription;
class TextLayout;
}

namespace lithium::mica {

// Forward declarations
class Context;
class Path;
class Brush;
class Texture;

// ============================================================================
// Paint (Brush + Composite Mode)
// ============================================================================>

/// Represents how colors are applied when drawing
struct Paint {
    std::unique_ptr<Brush> brush;
    BlendMode blend_mode{BlendMode::SourceOver};
    f32 opacity{1.0f};

    /// Default constructor
    Paint() = default;

    /// Copy constructor
    Paint(const Paint& other);

    /// Copy assignment
    Paint& operator=(const Paint& other);

    /// Move constructor and assignment
    Paint(Paint&&) noexcept = default;
    Paint& operator=(Paint&&) noexcept = default;

    /// Create a solid color paint
    static Paint solid(const Color& color);

    /// Create a linear gradient paint
    static Paint linear_gradient(
        Vec2 start,
        Vec2 end,
        const std::vector<GradientStop>& stops);

    /// Create a radial gradient paint
    static Paint radial_gradient(
        Vec2 center,
        f32 radius,
        const std::vector<GradientStop>& stops);

    /// Create an image/texture paint
    static Paint texture(Texture* texture);
};

// ============================================================================
// Brush
// ============================================================================

/// Abstract brush for filling shapes
class Brush {
public:
    virtual ~Brush() = default;

    /// Get brush type
    [[nodiscard]] virtual BrushType type() const noexcept = 0;

    /// Set transform matrix
    virtual void set_transform(const Mat3& transform) = 0;

    /// Get current transform
    [[nodiscard]] virtual const Mat3& transform() const noexcept = 0;

    /// Clone the brush (for copy semantics)
    [[nodiscard]] virtual std::unique_ptr<Brush> clone() const = 0;

protected:
    Brush() = default;
    Brush(const Brush&) = default;
    Brush& operator=(const Brush&) = default;
};

// Solid color brush
class SolidBrush : public Brush {
public:
    Color color;

    explicit SolidBrush(const Color& c) : color(c) {}

    [[nodiscard]] BrushType type() const noexcept override {
        return BrushType::Solid;
    }

    void set_transform(const Mat3& transform) override {
        m_transform = transform;
    }

    [[nodiscard]] const Mat3& transform() const noexcept override {
        return m_transform;
    }

    [[nodiscard]] std::unique_ptr<Brush> clone() const override {
        auto cloned = std::make_unique<SolidBrush>(color);
        cloned->m_transform = m_transform;
        return cloned;
    }

private:
    Mat3 m_transform = Mat3::identity();
};

// ============================================================================
// Path
// ============================================================================

/// 2D vector path for drawing shapes
class Path {
public:
    virtual ~Path() = default;

    /// Move to point without drawing
    virtual void move_to(Vec2 p) = 0;

    /// Line to point
    virtual void line_to(Vec2 p) = 0;

    /// Quadratic bezier curve
    virtual void quad_to(Vec2 control, Vec2 end) = 0;

    /// Cubic bezier curve
    virtual void cube_to(Vec2 control1, Vec2 control2, Vec2 end) = 0;

    /// Close current sub-path
    virtual void close() = 0;

    /// Add rectangle
    virtual void add_rect(const Rect& rect) = 0;

    /// Add rounded rectangle
    virtual void add_rounded_rect(const Rect& rect, f32 radius) = 0;

    /// Add ellipse
    virtual void add_ellipse(Vec2 center, f32 radius_x, f32 radius_y) = 0;

    /// Add circle
    virtual void add_circle(Vec2 center, f32 radius) = 0;

    /// Add arc
    virtual void add_arc(Vec2 center, f32 radius, f32 start_angle, f32 sweep_angle) = 0;

    /// Clear all path elements
    virtual void clear() = 0;

    /// Check if path is empty
    [[nodiscard]] virtual bool is_empty() const noexcept = 0;

    /// Get bounding box of path
    [[nodiscard]] virtual Rect bounding_box() const noexcept = 0;

    /// Create a copy of the path
    [[nodiscard]] virtual std::unique_ptr<Path> clone() const = 0;

protected:
    Path() = default;
};

/// Create a new path
[[nodiscard]] std::unique_ptr<Path> create_path();

// ============================================================================
// Painter State
// ============================================================================

struct PainterState {
    Mat3 transform;
    Paint paint;
    f32 line_width{1.0f};
    LineCap line_cap{LineCap::Butt};
    LineJoin line_join{LineJoin::Miter};
    f32 miter_limit{4.0f};

    /// Comparison for state stack
    [[nodiscard]] bool operator==(const PainterState& other) const {
        return transform == other.transform &&
               line_width == other.line_width &&
               line_cap == other.line_cap &&
               line_join == other.line_join &&
               miter_limit == other.miter_limit;
    }

    [[nodiscard]] bool operator!=(const PainterState& other) const {
        return !(*this == other);
    }
};

// ============================================================================
// Painter
// ============================================================================>

/// High-level drawing API
class Painter {
public:
    virtual ~Painter() = default;

    /// Get associated context
    [[nodiscard]] virtual Context* context() noexcept = 0;

    // ========================================================================
    // State Management
    // ========================================================================

    /// Save current state to stack
    virtual void save() = 0;

    /// Restore previous state from stack
    virtual void restore() = 0;

    /// Get current state
    [[nodiscard]] virtual const PainterState& state() const noexcept = 0;

    // ========================================================================
    // Transforms
    // ========================================================================

    /// Translate by offset
    virtual void translate(Vec2 offset) = 0;

    /// Scale by factors
    virtual void scale(Vec2 factors) = 0;

    /// Rotate by angle (radians)
    virtual void rotate(f32 angle) = 0;

    /// Multiply current transform by matrix
    virtual void concat(const Mat3& matrix) = 0;

    /// Set current transform
    virtual void set_transform(const Mat3& transform) = 0;

    /// Get current transform
    [[nodiscard]] virtual const Mat3& transform() const noexcept = 0;

    // ========================================================================
    // Drawing Primitives
    // ========================================================================

    /// Draw a line
    virtual void draw_line(Vec2 start, Vec2 end, const Paint& paint) = 0;

    /// Draw a rectangle
    virtual void draw_rect(const Rect& rect, const Paint& paint) = 0;

    /// Fill a rectangle
    virtual void fill_rect(const Rect& rect, const Paint& paint) = 0;

    /// Draw a rounded rectangle
    virtual void draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) = 0;

    /// Fill a rounded rectangle
    virtual void fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) = 0;

    /// Draw an ellipse
    virtual void draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) = 0;

    /// Fill an ellipse
    virtual void fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) = 0;

    /// Draw a circle
    virtual void draw_circle(Vec2 center, f32 radius, const Paint& paint) = 0;

    /// Fill a circle
    virtual void fill_circle(Vec2 center, f32 radius, const Paint& paint) = 0;

    /// Draw a path
    virtual void draw_path(const Path& path, const Paint& paint) = 0;

    /// Fill a path
    virtual void fill_path(const Path& path, const Paint& paint) = 0;

    // ========================================================================
    // Text Drawing (via Beryl)
    // ========================================================================

    /// Draw text at position
    virtual void draw_text(
        Vec2 position,
        const String& text,
        const Paint& paint,
        const beryl::FontDescription& font_desc) = 0;

    /// Draw text layout
    virtual void draw_text_layout(
        Vec2 position,
        const beryl::TextLayout& layout,
        const Paint& paint) = 0;

    // ========================================================================
    // Images
    // ========================================================================

    /// Draw an image/texture
    virtual void draw_image(
        Vec2 position,
        Texture* texture,
        const Paint& paint = {}) = 0;

    /// Draw a textured rectangle
    virtual void draw_image_rect(
        const Rect& dest,
        Texture* texture,
        const Rect& src,
        const Paint& paint = {}) = 0;

    /// Draw a textured rectangle with tint
    virtual void draw_image_tinted(
        const Rect& dest,
        Texture* texture,
        const Rect& src,
        const Color& tint) = 0;

    // ========================================================================
    // Clipping
    // ========================================================================

    /// Clip to rectangle
    virtual void clip_rect(const Rect& rect) = 0;

    /// Clip to path
    virtual void clip_path(const Path& path) = 0;

    /// Remove clipping
    virtual void reset_clip() = 0;

    // ========================================================================
    // Clear
    // ========================================================================

    /// Clear entire canvas with color
    virtual void clear(const Color& color) = 0;

protected:
    Painter() = default;
};

} // namespace lithium::mica
