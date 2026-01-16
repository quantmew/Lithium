#pragma once

#include "paint_context.hpp"
#include "display_list.hpp"
#include "lithium/layout/box.hpp"
#include <vector>
#include <memory>

namespace lithium::render {

// ============================================================================
// Render Layer Tree
// ============================================================================

enum class CompositingReason {
    None,
    Root,
    Transform3D,
    Opacity,
    Position,
    Overflow,
    Canvas,
    Video,
    WillChange,
};

class RenderLayer {
public:
    explicit RenderLayer(layout::LayoutBox* box);

    [[nodiscard]] layout::LayoutBox* box() const { return m_box; }
    [[nodiscard]] RenderLayer* parent() const { return m_parent; }
    [[nodiscard]] const std::vector<std::unique_ptr<RenderLayer>>& children() const { return m_children; }

    // Layer properties
    [[nodiscard]] bool needs_compositing() const { return m_compositing_reason != CompositingReason::None; }
    [[nodiscard]] CompositingReason compositing_reason() const { return m_compositing_reason; }
    void set_compositing_reason(CompositingReason reason) { m_compositing_reason = reason; }

    // Bounds
    [[nodiscard]] RectF bounds() const { return m_bounds; }
    void set_bounds(const RectF& bounds) { m_bounds = bounds; }

    // Opacity
    [[nodiscard]] f32 opacity() const { return m_opacity; }
    void set_opacity(f32 opacity) { m_opacity = opacity; }

    // Display list for this layer
    [[nodiscard]] const DisplayList& display_list() const { return m_display_list; }
    void set_display_list(DisplayList list) { m_display_list = std::move(list); }

    // Tree manipulation
    void add_child(std::unique_ptr<RenderLayer> child);

private:
    layout::LayoutBox* m_box;
    RenderLayer* m_parent{nullptr};
    std::vector<std::unique_ptr<RenderLayer>> m_children;

    CompositingReason m_compositing_reason{CompositingReason::None};
    RectF m_bounds;
    f32 m_opacity{1.0f};
    DisplayList m_display_list;
};

// ============================================================================
// Compositor - Manages layer tree and compositing
// ============================================================================

class Compositor {
public:
    Compositor();

    // Build layer tree from layout
    void build_layer_tree(layout::LayoutBox& root);

    // Update layer properties
    void update_layers();

    // Generate display lists for each layer
    void paint_layers();

    // Composite layers and render to screen
    void composite(platform::GraphicsContext& graphics, text::FontContext& fonts);

    // Get root layer
    [[nodiscard]] RenderLayer* root_layer() const { return m_root_layer.get(); }

    // Damage tracking
    void mark_dirty(const RectF& rect);
    void mark_all_dirty();
    [[nodiscard]] const std::vector<RectF>& dirty_rects() const { return m_dirty_rects; }
    void clear_dirty_rects() { m_dirty_rects.clear(); }

private:
    // Build layer for a box (recursive)
    std::unique_ptr<RenderLayer> build_layer(layout::LayoutBox& box, RenderLayer* parent);

    // Determine if a box needs its own layer
    [[nodiscard]] CompositingReason needs_compositing(const layout::LayoutBox& box) const;

    // Paint a single layer
    void paint_layer(RenderLayer& layer);

    // Composite a layer tree
    void composite_layer(RenderLayer& layer, PaintContext& context);

    std::unique_ptr<RenderLayer> m_root_layer;
    std::vector<RectF> m_dirty_rects;
};

// ============================================================================
// Frame - Represents a single rendered frame
// ============================================================================

struct Frame {
    DisplayList display_list;
    std::vector<RectF> damage_rects;
    u64 frame_id{0};
};

// ============================================================================
// Renderer - High-level rendering interface
// ============================================================================

class Renderer {
public:
    Renderer(platform::GraphicsContext& graphics);

    // Initialize renderer
    void init();

    // Render a frame
    void render(layout::LayoutBox& layout_root, text::FontContext& fonts);

    // Settings
    void set_background_color(const Color& color) { m_background_color = color; }
    void set_debug_paint_rects(bool enabled) { m_debug_paint_rects = enabled; }

    // Stats
    [[nodiscard]] u64 frame_count() const { return m_frame_count; }
    [[nodiscard]] f64 last_frame_time_ms() const { return m_last_frame_time_ms; }

private:
    platform::GraphicsContext& m_graphics;
    Compositor m_compositor;

    Color m_background_color{Color::white()};
    bool m_debug_paint_rects{false};

    u64 m_frame_count{0};
    f64 m_last_frame_time_ms{0};
};

} // namespace lithium::render
