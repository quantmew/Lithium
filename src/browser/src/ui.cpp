/**
 * Browser UI implementation
 */

#include "lithium/browser/engine.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/platform/graphics_context.hpp"

namespace lithium::browser {

// ============================================================================
// UI Constants
// ============================================================================

namespace ui {

constexpr f32 TOOLBAR_HEIGHT = 40.0f;
constexpr f32 TAB_BAR_HEIGHT = 30.0f;
constexpr f32 STATUS_BAR_HEIGHT = 20.0f;

constexpr Color TOOLBAR_BG{240, 240, 240, 255};
constexpr Color TAB_BAR_BG{230, 230, 230, 255};
constexpr Color STATUS_BAR_BG{245, 245, 245, 255};
constexpr Color BUTTON_BG{220, 220, 220, 255};
constexpr Color BUTTON_HOVER_BG{200, 200, 200, 255};
constexpr Color TEXT_COLOR{30, 30, 30, 255};
constexpr Color URL_BAR_BG{255, 255, 255, 255};
constexpr Color URL_BAR_BORDER{180, 180, 180, 255};

} // namespace ui

// ============================================================================
// BrowserUI class
// ============================================================================

class BrowserUI {
public:
    BrowserUI(Engine& engine, platform::Window& window)
        : m_engine(engine)
        , m_window(window)
    {
        auto size = window.size();
        m_width = static_cast<f32>(size.width);
        m_height = static_cast<f32>(size.height);
    }

    void resize(f32 width, f32 height) {
        m_width = width;
        m_height = height;
    }

    void render(platform::GraphicsContext& graphics) {
        // Draw tab bar
        render_tab_bar(graphics);

        // Draw toolbar
        render_toolbar(graphics);

        // Draw status bar
        render_status_bar(graphics);

        // Content area is handled by the engine
    }

    void handle_mouse_click(f32 x, f32 y) {
        // Check if click is in toolbar
        if (y < ui::TAB_BAR_HEIGHT + ui::TOOLBAR_HEIGHT) {
            handle_toolbar_click(x, y);
        }
    }

    // Get content area rect
    [[nodiscard]] RectF content_rect() const {
        return {
            0,
            ui::TAB_BAR_HEIGHT + ui::TOOLBAR_HEIGHT,
            m_width,
            m_height - ui::TAB_BAR_HEIGHT - ui::TOOLBAR_HEIGHT - ui::STATUS_BAR_HEIGHT
        };
    }

private:
    void render_tab_bar(platform::GraphicsContext& graphics) {
        // Tab bar background
        graphics.fill_rect({0, 0, m_width, ui::TAB_BAR_HEIGHT}, ui::TAB_BAR_BG);

        // Tab buttons (simplified)
        f32 tab_width = 150.0f;
        f32 x = 0;

        // Draw single tab for now
        graphics.fill_rect({x + 2, 2, tab_width - 4, ui::TAB_BAR_HEIGHT - 4}, ui::TOOLBAR_BG);
        graphics.draw_text({x + 10, ui::TAB_BAR_HEIGHT - 8}, m_engine.title(), ui::TEXT_COLOR, 12.0f);

        // New tab button
        x += tab_width;
        graphics.fill_rect({x + 2, 2, 30, ui::TAB_BAR_HEIGHT - 4}, ui::BUTTON_BG);
        graphics.draw_text({x + 10, ui::TAB_BAR_HEIGHT - 8}, "+"_s, ui::TEXT_COLOR, 14.0f);
    }

    void render_toolbar(platform::GraphicsContext& graphics) {
        f32 y = ui::TAB_BAR_HEIGHT;

        // Toolbar background
        graphics.fill_rect({0, y, m_width, ui::TOOLBAR_HEIGHT}, ui::TOOLBAR_BG);

        // Navigation buttons
        f32 x = 5;
        f32 btn_size = 30;

        // Back button
        graphics.fill_rect({x, y + 5, btn_size, btn_size},
                          m_engine.can_go_back() ? ui::BUTTON_BG : ui::TOOLBAR_BG);
        graphics.draw_text({x + 10, y + 25}, "<"_s, ui::TEXT_COLOR, 14.0f);
        x += btn_size + 5;

        // Forward button
        graphics.fill_rect({x, y + 5, btn_size, btn_size},
                          m_engine.can_go_forward() ? ui::BUTTON_BG : ui::TOOLBAR_BG);
        graphics.draw_text({x + 10, y + 25}, ">"_s, ui::TEXT_COLOR, 14.0f);
        x += btn_size + 5;

        // Reload button
        graphics.fill_rect({x, y + 5, btn_size, btn_size}, ui::BUTTON_BG);
        graphics.draw_text({x + 8, y + 25}, "R"_s, ui::TEXT_COLOR, 14.0f);
        x += btn_size + 10;

        // URL bar
        f32 url_width = m_width - x - 50;
        graphics.fill_rect({x, y + 5, url_width, 30}, ui::URL_BAR_BG);
        graphics.stroke_rect({x, y + 5, url_width, 30}, ui::URL_BAR_BORDER, 1.0f);
        graphics.draw_text({x + 10, y + 25}, m_engine.current_url(), ui::TEXT_COLOR, 12.0f);
    }

    void render_status_bar(platform::GraphicsContext& graphics) {
        f32 y = m_height - ui::STATUS_BAR_HEIGHT;

        // Status bar background
        graphics.fill_rect({0, y, m_width, ui::STATUS_BAR_HEIGHT}, ui::STATUS_BAR_BG);

        // Status text
        String status = m_engine.is_loading() ? "Loading..."_s : "Done"_s;
        graphics.draw_text({10, y + 14}, status, ui::TEXT_COLOR, 11.0f);
    }

    void handle_toolbar_click(f32 x, f32 y) {
        f32 toolbar_y = ui::TAB_BAR_HEIGHT;
        f32 btn_size = 30;

        // Check back button
        if (x >= 5 && x < 5 + btn_size && y >= toolbar_y + 5 && y < toolbar_y + 5 + btn_size) {
            m_engine.go_back();
            return;
        }

        // Check forward button
        f32 fwd_x = 5 + btn_size + 5;
        if (x >= fwd_x && x < fwd_x + btn_size && y >= toolbar_y + 5 && y < toolbar_y + 5 + btn_size) {
            m_engine.go_forward();
            return;
        }

        // Check reload button
        f32 reload_x = fwd_x + btn_size + 5;
        if (x >= reload_x && x < reload_x + btn_size && y >= toolbar_y + 5 && y < toolbar_y + 5 + btn_size) {
            m_engine.reload();
            return;
        }
    }

    Engine& m_engine;
    platform::Window& m_window;
    f32 m_width;
    f32 m_height;
};

} // namespace lithium::browser
