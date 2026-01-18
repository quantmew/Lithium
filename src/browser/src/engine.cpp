/**
 * Browser Engine implementation
 */

#include "lithium/browser/engine.hpp"
#include "lithium/html/tokenizer.hpp"
#include "lithium/html/tree_builder.hpp"
#include "lithium/render/display_list.hpp"
#include "lithium/render/paint_context.hpp"

namespace lithium::browser {

// ============================================================================
// Engine implementation
// ============================================================================

Engine::Engine() = default;

Engine::~Engine() = default;

bool Engine::init() {
    // Initialize subsystems
    if (!platform::platform::init()) {
        return false;
    }

    // Set up DOM bindings
    m_dom_bindings = std::make_unique<bindings::DOMBindings>(m_vm);
    m_dom_bindings->register_all();

    // Add user-agent stylesheet
    m_style_resolver.add_user_agent_stylesheet(css::default_user_agent_stylesheet());

    return true;
}

void Engine::load_url(const String& url) {
    if (m_on_load_started) {
        m_on_load_started(url);
    }

    m_is_loading = true;
    m_current_url = url;

    // Add to history
    if (m_history_index >= 0 && m_history_index < static_cast<i32>(m_history.size()) - 1) {
        // Clear forward history
        m_history.resize(m_history_index + 1);
    }
    m_history.push_back(url);
    m_history_index = static_cast<i32>(m_history.size()) - 1;

    // Set base URL for resource loading
    m_resource_loader.set_base_url(url);

    // Load the document
    auto result = m_resource_loader.load(url, network::ResourceType::Document);

    if (result.is_ok()) {
        parse_html_response(result.value().data_as_string());
        m_is_loading = false;

        if (m_on_load_finished) {
            m_on_load_finished(url, true);
        }
    } else {
        m_is_loading = false;

        if (m_on_load_finished) {
            m_on_load_finished(url, false);
        }
    }
}

void Engine::load_html(const String& html, const String& base_url) {
    m_current_url = base_url;
    m_resource_loader.set_base_url(base_url);

    if (m_on_load_started) {
        m_on_load_started(base_url);
    }

    parse_html_response(html);

    if (m_on_load_finished) {
        m_on_load_finished(base_url, true);
    }
}

void Engine::go_back() {
    if (can_go_back()) {
        --m_history_index;
        load_url(m_history[m_history_index]);
    }
}

void Engine::go_forward() {
    if (can_go_forward()) {
        ++m_history_index;
        load_url(m_history[m_history_index]);
    }
}

void Engine::reload() {
    if (!m_current_url.empty()) {
        load_url(m_current_url);
    }
}

void Engine::stop() {
    m_is_loading = false;
}

bool Engine::can_go_back() const {
    return m_history_index > 0;
}

bool Engine::can_go_forward() const {
    return m_history_index < static_cast<i32>(m_history.size()) - 1;
}

const String& Engine::title() const {
    static String empty;
    if (!m_document) {
        return empty;
    }
    return m_document->title();
}

void Engine::resize(i32 width, i32 height) {
    if (m_viewport_width != width || m_viewport_height != height) {
        m_viewport_width = width;
        m_viewport_height = height;
        invalidate_layout();
    }
}

void Engine::render(platform::GraphicsContext& graphics) {
    // Update layout if needed
    if (m_layout_dirty) {
        update_layout();
    }

    // Rebuild display list if needed
    if (m_render_dirty && m_layout_tree) {
        render::DisplayListBuilder builder;
        auto display_list = builder.build(*m_layout_tree);

        // Clear and render
        graphics.begin_frame();
        graphics.clear({255, 255, 255, 255});  // White background

        render::paint_display_list(graphics, display_list);

        graphics.end_frame();
        m_render_dirty = false;
    }
}

void Engine::execute_script(const String& script) {
    auto result = m_vm.interpret(script);
    if (result != js::VM::InterpretResult::Ok) {
        // Handle error
    }
}

void Engine::handle_event(const platform::Event& event) {
    platform::EventDispatcher dispatcher(event);

    dispatcher.dispatch<platform::WindowResizeEvent>([this](const auto& e) {
        resize(e.width, e.height);
        return true;
    });

    dispatcher.dispatch<platform::MouseButtonEvent>([this](const auto& e) {
        // Handle click - would need hit testing
        return true;
    });

    dispatcher.dispatch<platform::KeyEvent>([this](const auto& e) {
        // Handle keyboard input
        return true;
    });
}

void Engine::process_tasks() {
    // Process pending network callbacks, timers, etc.
}

void Engine::parse_html_response(const String& html) {
    // Parse HTML
    m_document = m_html_parser.parse(html);

    if (!m_document) {
        return;
    }

    // Set up DOM bindings
    m_dom_bindings->set_document(m_document);

    // Extract and apply stylesheets
    apply_stylesheets();

    // Execute scripts
    execute_scripts();

    // Build layout tree
    invalidate_layout();

    // Notify title change
    if (m_on_title_changed && !m_document->title().empty()) {
        m_on_title_changed(m_document->title());
    }
}

void Engine::apply_stylesheets() {
    if (!m_document) return;

    // Find all <style> elements
    auto style_elements = m_document->get_elements_by_tag_name("style"_s);
    for (auto* style : style_elements) {
        String css = style->text_content();
        auto stylesheet = m_css_parser.parse_stylesheet(css);
        m_style_resolver.add_stylesheet(stylesheet);
    }

    // Find all <link rel="stylesheet"> elements
    auto link_elements = m_document->get_elements_by_tag_name("link"_s);
    for (auto* link : link_elements) {
        auto rel = link->get_attribute("rel"_s);
        if (!rel || !rel->equals_ignore_case("stylesheet"_s)) {
            continue;
        }

        auto href = link->get_attribute("href"_s);
        if (!href || href->empty()) {
            continue;
        }

        auto result = m_resource_loader.load(*href, network::ResourceType::Stylesheet);
        if (result.is_ok()) {
            auto stylesheet = m_css_parser.parse_stylesheet(result.value().data_as_string());
            m_style_resolver.add_stylesheet(stylesheet);
        }
    }

    // Resolve styles for all elements
    m_style_resolver.resolve_document(*m_document);
}

void Engine::execute_scripts() {
    if (!m_document) return;

    // Find all <script> elements
    auto script_elements = m_document->get_elements_by_tag_name("script"_s);
    for (auto* script : script_elements) {
        if (auto src = script->get_attribute("src"_s); src && !src->empty()) {
            // External script
            auto result = m_resource_loader.load(*src, network::ResourceType::Script);
            if (result.is_ok()) {
                execute_script(result.value().data_as_string());
            }
        } else {
            // Inline script
            execute_script(script->text_content());
        }
    }
}

void Engine::update_layout() {
    if (!m_document) {
        m_layout_tree.reset();
        m_layout_dirty = false;
        return;
    }

    // Build layout tree
    layout::LayoutTreeBuilder builder;
    m_layout_tree = builder.build(*m_document, m_style_resolver);

    // Perform layout
    if (m_layout_tree) {
        layout::LayoutContext context{};
        context.containing_block_width = static_cast<f32>(m_viewport_width);
        context.containing_block_height = static_cast<f32>(m_viewport_height);
        context.viewport_width = static_cast<f32>(m_viewport_width);
        context.viewport_height = static_cast<f32>(m_viewport_height);
        context.root_font_size = 16.0f;
        context.font_context = &m_layout_engine.font_context();
        m_layout_engine.layout(*m_layout_tree, context);
    }

    m_layout_dirty = false;
    m_render_dirty = true;
}

void Engine::invalidate_layout() {
    m_layout_dirty = true;
    m_render_dirty = true;
}

void Engine::invalidate_render() {
    m_render_dirty = true;
}

} // namespace lithium::browser
