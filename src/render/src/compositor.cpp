/**
 * Compositor implementation (simplified)
 */

#include "lithium/render/compositor.hpp"
#include "lithium/render/display_list.hpp"

namespace lithium::render {

RenderLayer::RenderLayer(layout::LayoutBox* box)
    : m_box(box) {}

void RenderLayer::add_child(std::unique_ptr<RenderLayer> child) {
    child->m_parent = this;
    m_children.push_back(std::move(child));
}

Compositor::Compositor() = default;

void Compositor::build_layer_tree(layout::LayoutBox& root) {
    m_root_layer = build_layer(root, nullptr);
}

void Compositor::update_layers() {
    // Stub - full implementation would track geometry/opacity changes
}

void Compositor::paint_layers() {
    if (!m_root_layer || !m_root_layer->box()) {
        return;
    }

    DisplayListBuilder builder;
    auto list = builder.build(*m_root_layer->box());
    m_root_layer->set_display_list(std::move(list));
}

void Compositor::composite(platform::GraphicsContext& graphics, text::FontContext& fonts) {
    if (!m_root_layer) {
        return;
    }

    PaintContext context(graphics, fonts);
    composite_layer(*m_root_layer, context);
}

void Compositor::mark_dirty(const RectF& rect) {
    m_dirty_rects.push_back(rect);
}

void Compositor::mark_all_dirty() {
    m_dirty_rects.clear();
}

std::unique_ptr<RenderLayer> Compositor::build_layer(layout::LayoutBox& box, RenderLayer* parent) {
    auto layer = std::make_unique<RenderLayer>(&box);
    layer->set_parent(parent);
    layer->set_bounds(box.dimensions().border_box());
    layer->set_opacity(1.0f);

    for (auto& child : box.children()) {
        auto child_layer = build_layer(*child, layer.get());
        layer->add_child(std::move(child_layer));
    }

    return layer;
}

CompositingReason Compositor::needs_compositing(const layout::LayoutBox& box) const {
    (void)box;
    return CompositingReason::None;
}

void Compositor::paint_layer(RenderLayer& layer) {
    if (!layer.box()) {
        return;
    }

    DisplayListBuilder builder;
    auto list = builder.build(*layer.box());
    layer.set_display_list(std::move(list));
}

void Compositor::composite_layer(RenderLayer& layer, PaintContext& context) {
    context.execute(layer.display_list());

    for (auto& child : layer.children()) {
        composite_layer(*child, context);
    }
}

Renderer::Renderer(platform::GraphicsContext& graphics)
    : m_graphics(graphics) {}

void Renderer::init() {
    m_frame_count = 0;
    m_last_frame_time_ms = 0;
}

void Renderer::render(layout::LayoutBox& layout_root, text::FontContext& fonts) {
    m_compositor.mark_all_dirty();
    m_compositor.build_layer_tree(layout_root);
    m_compositor.paint_layers();
    m_compositor.composite(m_graphics, fonts);
    ++m_frame_count;
}

} // namespace lithium::render
