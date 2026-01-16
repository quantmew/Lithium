/**
 * Compositor implementation
 */

#include "lithium/render/compositor.hpp"
#include "lithium/render/display_list.hpp"
#include "lithium/platform/graphics_context.hpp"

namespace lithium::render {

// ============================================================================
// Layer
// ============================================================================

struct Layer {
    DisplayList display_list;
    RectF bounds;
    f32 opacity{1.0f};
    bool needs_repaint{true};

    // Transform
    f32 translate_x{0};
    f32 translate_y{0};
    f32 scale_x{1};
    f32 scale_y{1};

    // Compositing
    enum class BlendMode {
        Normal,
        Multiply,
        Screen,
        Overlay
    } blend_mode{BlendMode::Normal};

    // Cached texture (for GPU compositing)
    // void* texture{nullptr};
};

// ============================================================================
// Compositor
// ============================================================================

class Compositor {
public:
    Compositor() = default;

    // Create a new layer
    usize create_layer(const RectF& bounds) {
        Layer layer;
        layer.bounds = bounds;
        m_layers.push_back(std::move(layer));
        return m_layers.size() - 1;
    }

    // Update layer content
    void update_layer(usize index, DisplayList display_list) {
        if (index < m_layers.size()) {
            m_layers[index].display_list = std::move(display_list);
            m_layers[index].needs_repaint = true;
        }
    }

    // Set layer transform
    void set_layer_transform(usize index, f32 tx, f32 ty, f32 sx, f32 sy) {
        if (index < m_layers.size()) {
            auto& layer = m_layers[index];
            layer.translate_x = tx;
            layer.translate_y = ty;
            layer.scale_x = sx;
            layer.scale_y = sy;
        }
    }

    // Set layer opacity
    void set_layer_opacity(usize index, f32 opacity) {
        if (index < m_layers.size()) {
            m_layers[index].opacity = opacity;
        }
    }

    // Remove a layer
    void remove_layer(usize index) {
        if (index < m_layers.size()) {
            m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    // Composite all layers to screen
    void composite(platform::GraphicsContext& graphics) {
        for (auto& layer : m_layers) {
            if (layer.display_list.empty()) continue;

            // Apply layer transform
            graphics.push_transform();
            graphics.translate(layer.translate_x, layer.translate_y);
            graphics.scale(layer.scale_x, layer.scale_y);

            // Apply layer opacity
            graphics.push_opacity(layer.opacity);

            // Paint layer content
            for (const auto& cmd : layer.display_list) {
                paint_command(graphics, cmd);
            }

            graphics.pop_opacity();
            graphics.pop_transform();
        }
    }

    // Check if any layer needs repaint
    bool needs_repaint() const {
        for (const auto& layer : m_layers) {
            if (layer.needs_repaint) return true;
        }
        return false;
    }

    // Mark all layers as painted
    void mark_painted() {
        for (auto& layer : m_layers) {
            layer.needs_repaint = false;
        }
    }

private:
    void paint_command(platform::GraphicsContext& graphics, const DisplayCommand& cmd) {
        std::visit([&graphics](auto&& command) {
            using T = std::decay_t<decltype(command)>;
            if constexpr (std::is_same_v<T, FillRectCommand>) {
                graphics.fill_rect(command.rect, command.color);
            } else if constexpr (std::is_same_v<T, StrokeRectCommand>) {
                graphics.stroke_rect(command.rect, command.color, command.width);
            } else if constexpr (std::is_same_v<T, DrawLineCommand>) {
                graphics.draw_line(command.from, command.to, command.color, command.width);
            } else if constexpr (std::is_same_v<T, DrawTextCommand>) {
                graphics.draw_text(command.position, command.text, command.color, command.font_size);
            } else if constexpr (std::is_same_v<T, PushClipCommand>) {
                graphics.push_clip(command.rect);
            } else if constexpr (std::is_same_v<T, PopClipCommand>) {
                graphics.pop_clip();
            } else if constexpr (std::is_same_v<T, PushOpacityCommand>) {
                graphics.push_opacity(command.opacity);
            } else if constexpr (std::is_same_v<T, PopOpacityCommand>) {
                graphics.pop_opacity();
            } else if constexpr (std::is_same_v<T, PushTransformCommand>) {
                graphics.push_transform();
                graphics.translate(command.translate_x, command.translate_y);
                graphics.scale(command.scale_x, command.scale_y);
                graphics.rotate(command.rotate);
            } else if constexpr (std::is_same_v<T, PopTransformCommand>) {
                graphics.pop_transform();
            }
        }, cmd);
    }

    std::vector<Layer> m_layers;
};

// ============================================================================
// Public compositor instance management
// ============================================================================

static std::unique_ptr<Compositor> g_compositor;

void init_compositor() {
    g_compositor = std::make_unique<Compositor>();
}

void shutdown_compositor() {
    g_compositor.reset();
}

Compositor* get_compositor() {
    return g_compositor.get();
}

} // namespace lithium::render
