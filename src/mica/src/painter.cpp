/**
 * Mica Graphics Engine - Painter Implementation
 */

#include "lithium/mica/painter.hpp"

namespace lithium::mica {

// ============================================================================
// Paint Copy Constructor / Assignment
// ============================================================================

Paint::Paint(const Paint& other)
    : blend_mode(other.blend_mode)
    , opacity(other.opacity) {
    if (other.brush) {
        brush = other.brush->clone();
    }
}

Paint& Paint::operator=(const Paint& other) {
    if (this != &other) {
        blend_mode = other.blend_mode;
        opacity = other.opacity;
        if (other.brush) {
            brush = other.brush->clone();
        } else {
            brush.reset();
        }
    }
    return *this;
}

// ============================================================================
// Paint Static Factory Methods
// ============================================================================

Paint Paint::solid(const Color& color) {
    Paint paint;
    paint.brush = std::make_unique<SolidBrush>(color);
    paint.blend_mode = BlendMode::SourceOver;
    paint.opacity = 1.0f;
    return paint;
}

Paint Paint::linear_gradient(
    Vec2 start,
    Vec2 end,
    const std::vector<GradientStop>& stops) {
    // TODO: Implement gradient brush
    Paint paint;
    paint.blend_mode = BlendMode::SourceOver;
    paint.opacity = 1.0f;
    return paint;
}

Paint Paint::radial_gradient(
    Vec2 center,
    f32 radius,
    const std::vector<GradientStop>& stops) {
    // TODO: Implement radial gradient brush
    Paint paint;
    paint.blend_mode = BlendMode::SourceOver;
    paint.opacity = 1.0f;
    return paint;
}

Paint Paint::texture(Texture* texture) {
    // TODO: Implement texture brush
    Paint paint;
    paint.blend_mode = BlendMode::SourceOver;
    paint.opacity = 1.0f;
    return paint;
}

} // namespace lithium::mica
