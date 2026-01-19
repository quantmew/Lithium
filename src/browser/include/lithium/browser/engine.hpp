#pragma once

#include "lithium/core/types.hpp"
#include "lithium/dom/document.hpp"
#include "lithium/html/parser.hpp"
#include "lithium/css/style_resolver.hpp"
#include "lithium/js/vm.hpp"
#include "lithium/layout/box.hpp"
#include "lithium/layout/layout_context.hpp"
#include "lithium/network/resource_loader.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/bindings/dom_bindings.hpp"

// Mica graphics engine
#include "lithium/mica/mica.hpp"
#include "lithium/beryl/beryl.hpp"

namespace lithium::browser {

// ============================================================================
// Browser Engine - Coordinates all components
// ============================================================================

class Engine {
public:
    Engine();
    ~Engine();

    // Initialize the engine
    bool init();

    // Load a URL or HTML string
    void load_url(const String& url);
    void load_html(const String& html, const String& base_url = "about:blank");

    // Navigation
    void go_back();
    void go_forward();
    void reload();
    void stop();

    [[nodiscard]] bool can_go_back() const;
    [[nodiscard]] bool can_go_forward() const;

    // Current state
    [[nodiscard]] const String& current_url() const { return m_current_url; }
    [[nodiscard]] const String& title() const;
    [[nodiscard]] bool is_loading() const { return m_is_loading; }

    // Document access
    [[nodiscard]] dom::Document* document() const { return m_document.get(); }

    // Rendering
    void resize(i32 width, i32 height);
    void render();  // Renders using mica engine

    // Graphics setup (called by main to pass mica components)
    void set_graphics_context(
        std::unique_ptr<mica::Context> context,
        std::unique_ptr<mica::Painter> painter);

    // JavaScript
    void execute_script(const String& script);
    [[nodiscard]] js::VM& vm() { return m_vm; }

    // Events
    void handle_event(const platform::Event& event);

    // Process pending tasks (timers, network callbacks, etc.)
    void process_tasks();

    // Callbacks
    using TitleChangedCallback = std::function<void(const String& title)>;
    using LoadStartedCallback = std::function<void(const String& url)>;
    using LoadFinishedCallback = std::function<void(const String& url, bool success)>;
    using NavigationCallback = std::function<void(const String& url)>;

    void set_title_changed_callback(TitleChangedCallback cb) { m_on_title_changed = std::move(cb); }
    void set_load_started_callback(LoadStartedCallback cb) { m_on_load_started = std::move(cb); }
    void set_load_finished_callback(LoadFinishedCallback cb) { m_on_load_finished = std::move(cb); }
    void set_navigation_callback(NavigationCallback cb) { m_on_navigation = std::move(cb); }

private:
    // Document loading
    void parse_html_response(const String& html);
    void apply_stylesheets();
    void execute_scripts();

    // Layout and rendering
    void update_layout();
    void invalidate_layout();
    void invalidate_render();

    // Rendering helpers
    void render_layout_box(mica::Painter& painter, const layout::LayoutBox& box);

    // Document
    RefPtr<dom::Document> m_document;

    // Parsers
    html::Parser m_html_parser;
    css::Parser m_css_parser;

    // Styling
    css::StyleResolver m_style_resolver;

    // JavaScript
    js::VM m_vm;
    std::unique_ptr<bindings::DOMBindings> m_dom_bindings;

    // Layout
    std::unique_ptr<layout::LayoutBox> m_layout_tree;
    layout::LayoutEngine m_layout_engine;

    // Graphics (Mica)
    // Note: mica::Engine is owned by main.cpp, Engine only holds context and painter
    std::unique_ptr<mica::Context> m_graphics_context;
    std::unique_ptr<mica::Painter> m_painter;
    i32 m_viewport_width{800};
    i32 m_viewport_height{600};

    // Network
    network::ResourceLoader m_resource_loader;

    // Navigation
    String m_current_url;
    std::vector<String> m_history;
    i32 m_history_index{-1};

    // State
    bool m_is_loading{false};
    bool m_layout_dirty{true};
    bool m_render_dirty{true};

    // Callbacks
    TitleChangedCallback m_on_title_changed;
    LoadStartedCallback m_on_load_started;
    LoadFinishedCallback m_on_load_finished;
    NavigationCallback m_on_navigation;
};

} // namespace lithium::browser
