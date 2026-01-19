/**
 * OpenGL Painter Implementation
 */

#include "gl_backend.hpp"
#include <iostream>

namespace lithium::mica::opengl {

// ============================================================================
// GLPainter
// ============================================================================

GLPainter::GLPainter(GLContext& context)
    : m_context(context)
{
    // Initialize default state
    m_current_state.transform = Mat3::identity();
    m_current_state.line_width = 1.0f;
    m_current_state.line_cap = LineCap::Butt;
    m_current_state.line_join = LineJoin::Miter;
    m_current_state.miter_limit = 4.0f;

    // Set default OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GLPainter::~GLPainter() = default;

Context* GLPainter::context() noexcept {
    return &m_context;
}

void GLPainter::save() {
    m_state_stack.push_back(m_current_state);
}

void GLPainter::restore() {
    if (!m_state_stack.empty()) {
        m_current_state = m_state_stack.back();
        m_state_stack.pop_back();

        // Restore transform
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        const Mat3& mat = m_current_state.transform;
        GLfloat gl_mat[16] = {
            mat.m[0][0], mat.m[1][0], 0.0f, 0.0f,
            mat.m[0][1], mat.m[1][1], 0.0f, 0.0f,
            0.0f,         0.0f,         1.0f, 0.0f,
            mat.m[2][0], mat.m[2][1], 0.0f, 1.0f
        };
        glLoadMatrixf(gl_mat);
    }
}

const PainterState& GLPainter::state() const noexcept {
    return m_current_state;
}

void GLPainter::translate(Vec2 offset) {
    Mat3 translation = Mat3::translation(offset.x, offset.y);
    m_current_state.transform = translation * m_current_state.transform;

    glTranslatef(offset.x, offset.y, 0.0f);
}

void GLPainter::scale(Vec2 factors) {
    Mat3 scaling = Mat3::scale(factors.x, factors.y);
    m_current_state.transform = scaling * m_current_state.transform;

    glScalef(factors.x, factors.y, 1.0f);
}

void GLPainter::rotate(f32 angle) {
    Mat3 rotation = Mat3::rotation(angle);
    m_current_state.transform = rotation * m_current_state.transform;

    glRotatef(angle * 180.0f / 3.14159265359f, 0.0f, 0.0f, 1.0f);
}

void GLPainter::concat(const Mat3& matrix) {
    m_current_state.transform = matrix * m_current_state.transform;
    // TODO: Update GL matrix
}

void GLPainter::set_transform(const Mat3& transform) {
    m_current_state.transform = transform;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const Mat3& mat = transform;
    GLfloat gl_mat[16] = {
        mat.m[0][0], mat.m[1][0], 0.0f, 0.0f,
        mat.m[0][1], mat.m[1][1], 0.0f, 0.0f,
        0.0f,         0.0f,         1.0f, 0.0f,
        mat.m[2][0], mat.m[2][1], 0.0f, 1.0f
    };
    glLoadMatrixf(gl_mat);
}

const Mat3& GLPainter::transform() const noexcept {
    return m_current_state.transform;
}

void GLPainter::draw_line(Vec2 start, Vec2 end, const Paint& paint) {
    apply_brush(paint);

    glBegin(GL_LINES);
    glVertex2f(start.x, start.y);
    glVertex2f(end.x, end.y);
    glEnd();
}

void GLPainter::draw_rect(const Rect& rect, const Paint& paint) {
    apply_brush(paint);

    GLfloat x1 = rect.x;
    GLfloat y1 = rect.y;
    GLfloat x2 = rect.x + rect.width;
    GLfloat y2 = rect.y + rect.height;

    glBegin(GL_LINE_LOOP);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void GLPainter::fill_rect(const Rect& rect, const Paint& paint) {
    apply_brush(paint);

    GLfloat x1 = rect.x;
    GLfloat y1 = rect.y;
    GLfloat x2 = rect.x + rect.width;
    GLfloat y2 = rect.y + rect.height;

    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void GLPainter::draw_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    // TODO: Implement rounded rectangle
    draw_rect(rect, paint);
}

void GLPainter::fill_rounded_rect(const Rect& rect, f32 radius, const Paint& paint) {
    // TODO: Implement filled rounded rectangle
    fill_rect(rect, paint);
}

void GLPainter::draw_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    apply_brush(paint);

    const int segments = 64;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * 3.14159265359f * i / segments;
        float x = center.x + radius_x * std::cos(theta);
        float y = center.y + radius_y * std::sin(theta);
        glVertex2f(x, y);
    }
    glEnd();
}

void GLPainter::fill_ellipse(Vec2 center, f32 radius_x, f32 radius_y, const Paint& paint) {
    apply_brush(paint);

    const int segments = 64;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);  // Center point
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * 3.14159265359f * i / segments;
        float x = center.x + radius_x * std::cos(theta);
        float y = center.y + radius_y * std::sin(theta);
        glVertex2f(x, y);
    }
    glEnd();
}

void GLPainter::draw_circle(Vec2 center, f32 radius, const Paint& paint) {
    draw_ellipse(center, radius, radius, paint);
}

void GLPainter::fill_circle(Vec2 center, f32 radius, const Paint& paint) {
    fill_ellipse(center, radius, radius, paint);
}

void GLPainter::draw_path(const Path& path, const Paint& paint) {
    // TODO: Implement path rendering
    std::cerr << "GLPainter: Path rendering not yet implemented" << std::endl;
}

void GLPainter::fill_path(const Path& path, const Paint& paint) {
    // TODO: Implement path filling
    std::cerr << "GLPainter: Path filling not yet implemented" << std::endl;
}

void GLPainter::draw_text(
    Vec2 position,
    const String& text,
    const Paint& paint,
    const beryl::FontDescription& font_desc) {

    // TODO: Implement text rendering
    std::cerr << "GLPainter: Text rendering not yet implemented" << std::endl;
}

void GLPainter::draw_text_layout(
    Vec2 position,
    const beryl::TextLayout& layout,
    const Paint& paint) {

    // TODO: Implement text layout rendering
    std::cerr << "GLPainter: Text layout rendering not yet implemented" << std::endl;
}

void GLPainter::draw_image(Vec2 position, Texture* texture, const Paint& paint) {
    if (!texture) return;

    Rect dest_rect = {
        position.x,
        position.y,
        static_cast<f32>(texture->size().width),
        static_cast<f32>(texture->size().height)
    };

    Rect src_rect = {
        0,
        0,
        static_cast<f32>(texture->size().width),
        static_cast<f32>(texture->size().height)
    };

    draw_image_rect(dest_rect, texture, src_rect, paint);
}

void GLPainter::draw_image_rect(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Paint& paint) {

    // TODO: Implement textured quad rendering
    std::cerr << "GLPainter: Image rendering not yet implemented" << std::endl;
}

void GLPainter::draw_image_tinted(
    const Rect& dest,
    Texture* texture,
    const Rect& src,
    const Color& tint) {

    // TODO: Implement tinted image rendering
    std::cerr << "GLPainter: Tinted image rendering not yet implemented" << std::endl;
}

void GLPainter::clip_rect(const Rect& rect) {
    // TODO: Implement scissor rect clipping
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        static_cast<GLint>(rect.x),
        static_cast<GLint>(rect.y),
        static_cast<GLsizei>(rect.width),
        static_cast<GLsizei>(rect.height)
    );
}

void GLPainter::clip_path(const Path& path) {
    // TODO: Implement stencil buffer path clipping
    std::cerr << "GLPainter: Path clipping not yet implemented" << std::endl;
}

void GLPainter::reset_clip() {
    glDisable(GL_SCISSOR_TEST);
}

void GLPainter::clear(const Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLPainter::apply_brush(const Paint& paint) {
    if (!paint.brush) {
        // Default white color
        glColor4f(1.0f, 1.0f, 1.0f, paint.opacity);
        return;
    }

    if (paint.brush->type() == BrushType::Solid) {
        auto* solid = static_cast<SolidBrush*>(paint.brush.get());
        glColor4f(solid->color.r, solid->color.g, solid->color.b,
                 solid->color.a * paint.opacity);
    } else {
        // TODO: Handle gradient and image brushes
        glColor4f(1.0f, 1.0f, 1.0f, paint.opacity);
    }
}

} // namespace lithium::mica::opengl
