/**
 * Browser Engine implementation
 */

#include "lithium/browser/engine.hpp"
#include "lithium/html/tokenizer.hpp"
#include "lithium/html/tree_builder.hpp"
#include "lithium/core/logger.hpp"

namespace lithium::browser {

// ============================================================================
// Engine implementation
// ============================================================================

Engine::Engine() = default;

Engine::~Engine() = default;

bool Engine::init() {
    // Note: Graphics engine (mica) is created and owned by main.cpp
    // Engine only receives graphics context and painter via set_graphics_context()

    LITHIUM_LOG_INFO("Browser Engine initialization starting...");

    // Initialize subsystems
    if (!platform::platform::init()) {
        LITHIUM_LOG_ERROR("Failed to initialize platform subsystem");
        return false;
    }

    LITHIUM_LOG_INFO("Platform subsystem initialized");

    // Set up DOM bindings
    m_dom_bindings = std::make_unique<bindings::DOMBindings>(m_vm);
    m_dom_bindings->register_all();
    LITHIUM_LOG_INFO("DOM bindings registered");

    // Add user-agent stylesheet
    m_style_resolver.add_user_agent_stylesheet(css::default_user_agent_stylesheet());
    LITHIUM_LOG_INFO("User-agent stylesheet added");

    LITHIUM_LOG_INFO("Browser Engine initialized successfully");
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

    LITHIUM_LOG_INFO("Engine::load_html: loading {} bytes from {}", html.length(), base_url);

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

        // Note: In a full implementation, we'd need to recreate the context
        // For now, just mark layout as dirty
        invalidate_layout();
    }
}

void Engine::render() {
    if (!m_graphics_context || !m_painter) {
        LITHIUM_LOG_WARN("Engine::render: graphics context or painter not initialized (call set_graphics_context first)");
        return;
    }

    static int render_call_count = 0;
    if (render_call_count < 3) {
        LITHIUM_LOG_INFO_FMT("Engine::render called (count={}, layout_dirty={}, render_dirty={}, has_layout_tree={})",
            render_call_count, m_layout_dirty, m_render_dirty, m_layout_tree != nullptr);
        render_call_count++;
    }

    // Update layout if needed
    if (m_layout_dirty) {
        LITHIUM_LOG_INFO("Engine::render: updating layout (dirty)");
        update_layout();
    }

    // Begin frame
    m_graphics_context->begin_frame();

    // Clear background (white)
    m_painter->clear({1.0f, 1.0f, 1.0f, 1.0f});

    // Render layout tree if available
    if (m_layout_tree) {
        LITHIUM_LOG_INFO("Engine::render: rendering layout tree with mica");
        render_layout_box(*m_painter, *m_layout_tree);
    }

    // End frame
    m_graphics_context->end_frame();

    // Present the rendered image to the screen
    m_graphics_context->present();

    m_render_dirty = false;
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
    LITHIUM_LOG_INFO("Engine::parse_html_response: parsing {} bytes of HTML", html.length());

    // Parse HTML
    m_document = m_html_parser.parse(html);

    if (!m_document) {
        LITHIUM_LOG_ERROR("Engine::parse_html_response: failed to parse HTML document");
        return;
    }

    LITHIUM_LOG_INFO("Engine::parse_html_response: HTML parsed successfully");

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
        LITHIUM_LOG_WARN("Engine::update_layout: no document, clearing layout tree");
        m_layout_tree.reset();
        m_layout_dirty = false;
        return;
    }

    LITHIUM_LOG_INFO("Engine::update_layout: building layout tree (viewport: {}x{})",
        m_viewport_width, m_viewport_height);

    // IMPORTANT: Invalidate all cached styles to ensure UA stylesheet is applied
    m_style_resolver.invalidate_all();

    // Build layout tree
    layout::LayoutTreeBuilder builder;
    m_layout_tree = builder.build(*m_document, m_style_resolver);

    if (!m_layout_tree) {
        LITHIUM_LOG_ERROR("Engine::update_layout: failed to build layout tree");
        m_layout_dirty = false;
        return;
    }

    LITHIUM_LOG_INFO("Engine::update_layout: layout tree built successfully");

    // Perform layout
    layout::LayoutContext context{};
    context.containing_block_width = static_cast<f32>(m_viewport_width);
    context.containing_block_height = static_cast<f32>(m_viewport_height);
    context.viewport_width = static_cast<f32>(m_viewport_width);
    context.viewport_height = static_cast<f32>(m_viewport_height);
    context.root_font_size = 16.0f;
    context.font_backend = m_layout_engine.font_backend();
    m_layout_engine.layout(*m_layout_tree, context);

    LITHIUM_LOG_INFO("Engine::update_layout: layout completed");

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

// ============================================================================
// Mica Rendering Implementation
// ============================================================================

void Engine::render_layout_box(mica::Painter& painter, const layout::LayoutBox& box) {
    const auto& d = box.dimensions();
    const auto& style = box.style();

    // Skip if zero size
    if (d.content.width <= 0 || d.content.height <= 0) {
        return;
    }

    // Convert CSS color to mica Color
    auto css_to_mica_color = [](const lithium::Color& css_color) -> lithium::mica::Color {
        return {
            static_cast<f32>(css_color.r) / 255.0f,
            static_cast<f32>(css_color.g) / 255.0f,
            static_cast<f32>(css_color.b) / 255.0f,
            static_cast<f32>(css_color.a) / 255.0f
        };
    };

    // Get background color
    lithium::mica::Color bg_color;
    if (style.background_color.a == 0) {
        // Transparent - use light gray
        bg_color = {0.95f, 0.95f, 0.95f, 1.0f};
    } else {
        bg_color = css_to_mica_color(style.background_color);
    }

    // Create paint for background
    auto bg_paint = mica::Paint::solid(bg_color);

    // Draw background rectangle
    mica::Rect bg_rect{
        d.content.x,
        d.content.y,
        d.content.width,
        d.content.height
    };
    painter.fill_rect(bg_rect, bg_paint);

    // Draw border if any
    if (d.border.left > 0 || d.border.top > 0 || d.border.right > 0 || d.border.bottom > 0) {
        auto border_color = css_to_mica_color(style.border_top_color);
        auto border_paint = mica::Paint::solid(border_color);
        f32 border_width = d.border.left;
        painter.draw_rect(bg_rect, border_paint);
    }

    // Draw text content
    if (box.is_text() && !box.text().empty()) {
        // Get font size
        f32 font_size = 16.0f;  // Default
        if (style.font_size.unit == lithium::css::LengthUnit::Px) {
            font_size = static_cast<f32>(style.font_size.value);
        }

        // Get text color
        lithium::mica::Color text_color;
        if (style.color.a == 0) {
            text_color = {0.0f, 0.0f, 0.0f, 1.0f};  // Default black
        } else {
            text_color = css_to_mica_color(style.color);
        }

        // Create font description
        beryl::FontDescription font_desc;
        font_desc.size = font_size;
        font_desc.family = style.font_family.empty() ? String("Arial") : style.font_family[0];
        font_desc.weight = (style.font_weight == css::FontWeight::Bold ||
                           style.font_weight == css::FontWeight::W700)
            ? beryl::FontWeight::Bold : beryl::FontWeight::Normal;
        font_desc.style = (style.font_style == css::FontStyle::Italic)
            ? beryl::FontStyle::Italic : beryl::FontStyle::Normal;

        // Draw text at position
        mica::Vec2 text_pos{d.content.x, d.content.y + font_size * 0.8f};
        painter.draw_text(text_pos, box.text(), mica::Paint::solid(text_color), font_desc);
    }

    // Recursively render children
    for (const auto& child : box.children()) {
        render_layout_box(painter, *child);
    }
}

void Engine::set_graphics_context(
    std::unique_ptr<mica::Context> context,
    std::unique_ptr<mica::Painter> painter)
{
    m_graphics_context = std::move(context);
    m_painter = std::move(painter);
    LITHIUM_LOG_INFO("Graphics context and painter set in Engine");
}

} // namespace lithium::browser
