/**
 * DOM Bindings implementation - Expose DOM API to JavaScript
 */

#include "lithium/bindings/dom_bindings.hpp"
#include "lithium/dom/element.hpp"
#include "lithium/dom/text.hpp"
#include <chrono>

namespace lithium::bindings {

// ============================================================================
// DOMBindings implementation
// ============================================================================

DOMBindings::DOMBindings(js::VM& vm) : m_vm(vm) {}

void DOMBindings::register_all() {
    register_window();
    register_document();
    register_element();
    register_node();
    register_text();
    register_event();
    register_event_target();
}

void DOMBindings::set_document(RefPtr<dom::Document> document) {
    m_document = std::move(document);
    m_node_wrappers.clear();
}

void DOMBindings::register_window() {
    // window.alert
    m_vm.define_native("alert"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (!args.empty()) {
            // In a real implementation, this would show a dialog
            // For now, just log
        }
        return js::Value::undefined();
    });

    // window.console (will be set up by ConsoleBindings)
}

void DOMBindings::register_document() {
    m_vm.define_native("document"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (!m_document) {
            return js::Value::null();
        }
        return wrap_node(m_document.get());
    });
}

void DOMBindings::register_element() {
    // Element methods would be added to element wrapper objects
    // This is a simplified implementation
}

void DOMBindings::register_node() {
    // Node methods
}

void DOMBindings::register_text() {
    // Text node methods
}

void DOMBindings::register_event() {
    // Event constructor and methods
}

void DOMBindings::register_event_target() {
    // addEventListener, removeEventListener, dispatchEvent
}

js::Value DOMBindings::wrap_node(dom::Node* node) {
    if (!node) {
        return js::Value::null();
    }

    // Check cache
    auto it = m_node_wrappers.find(node);
    if (it != m_node_wrappers.end()) {
        return js::Value(it->second);
    }

    // Create wrapper object
    auto wrapper = std::make_shared<js::Object>();

    // Add common node properties
    wrapper->set_property("nodeType"_s, js::Value(static_cast<f64>(static_cast<int>(node->node_type()))));
    wrapper->set_property("nodeName"_s, js::Value(node->node_name()));

    if (auto* element = dynamic_cast<dom::Element*>(node)) {
        // Element-specific properties
        wrapper->set_property("tagName"_s, js::Value(element->tag_name()));
        wrapper->set_property("id"_s, js::Value(element->id()));
        wrapper->set_property("className"_s, js::Value(element->class_name()));

        // innerHTML (simplified - getter only)
        wrapper->set_property("innerHTML"_s, js::Value(element->inner_html()));
    } else if (auto* text = dynamic_cast<dom::Text*>(node)) {
        wrapper->set_property("textContent"_s, js::Value(text->data()));
    }

    // Cache and return
    m_node_wrappers[node] = wrapper;
    return js::Value(wrapper);
}

dom::Node* DOMBindings::unwrap_node(const js::Value& value) {
    // Look up in reverse map
    for (const auto& [node, wrapper] : m_node_wrappers) {
        if (value.as_object() == wrapper.get()) {
            return node;
        }
    }
    return nullptr;
}

// ============================================================================
// ConsoleBindings implementation
// ============================================================================

ConsoleBindings::ConsoleBindings(js::VM& vm) : m_vm(vm) {}

void ConsoleBindings::register_console() {
    // console.log
    m_vm.define_native("log"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        String message;
        for (const auto& arg : args) {
            if (!message.empty()) message += " "_s;
            message += arg.to_string();
        }
        if (m_log_callback) {
            m_log_callback("log"_s, message);
        }
        return js::Value::undefined();
    });

    // console.warn
    m_vm.define_native("warn"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        String message;
        for (const auto& arg : args) {
            if (!message.empty()) message += " "_s;
            message += arg.to_string();
        }
        if (m_log_callback) {
            m_log_callback("warn"_s, message);
        }
        return js::Value::undefined();
    });

    // console.error
    m_vm.define_native("error"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        String message;
        for (const auto& arg : args) {
            if (!message.empty()) message += " "_s;
            message += arg.to_string();
        }
        if (m_log_callback) {
            m_log_callback("error"_s, message);
        }
        return js::Value::undefined();
    });
}

// ============================================================================
// TimerBindings implementation
// ============================================================================

TimerBindings::TimerBindings(js::VM& vm) : m_vm(vm) {}

void TimerBindings::register_timers() {
    // setTimeout
    m_vm.define_native("setTimeout"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (args.empty() || !args[0].is_function()) {
            return js::Value::undefined();
        }

        f64 delay = args.size() > 1 ? args[1].to_number() : 0;
        auto now = std::chrono::steady_clock::now();
        f64 current_time = std::chrono::duration<f64, std::milli>(now.time_since_epoch()).count();

        Timer timer;
        timer.id = m_next_timer_id++;
        timer.callback = args[0];
        timer.interval_ms = delay;
        timer.next_fire_time = current_time + delay;
        timer.is_interval = false;

        m_timers.push_back(timer);

        return js::Value(static_cast<f64>(timer.id));
    });

    // setInterval
    m_vm.define_native("setInterval"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (args.empty() || !args[0].is_function()) {
            return js::Value::undefined();
        }

        f64 interval = args.size() > 1 ? args[1].to_number() : 0;
        auto now = std::chrono::steady_clock::now();
        f64 current_time = std::chrono::duration<f64, std::milli>(now.time_since_epoch()).count();

        Timer timer;
        timer.id = m_next_timer_id++;
        timer.callback = args[0];
        timer.interval_ms = interval;
        timer.next_fire_time = current_time + interval;
        timer.is_interval = true;

        m_timers.push_back(timer);

        return js::Value(static_cast<f64>(timer.id));
    });

    // clearTimeout
    m_vm.define_native("clearTimeout"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (args.empty()) return js::Value::undefined();

        u32 id = static_cast<u32>(args[0].to_number());
        m_timers.erase(
            std::remove_if(m_timers.begin(), m_timers.end(),
                [id](const Timer& t) { return t.id == id; }),
            m_timers.end());

        return js::Value::undefined();
    });

    // clearInterval
    m_vm.define_native("clearInterval"_s, [this](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        if (args.empty()) return js::Value::undefined();

        u32 id = static_cast<u32>(args[0].to_number());
        m_timers.erase(
            std::remove_if(m_timers.begin(), m_timers.end(),
                [id](const Timer& t) { return t.id == id; }),
            m_timers.end());

        return js::Value::undefined();
    });
}

void TimerBindings::process_timers() {
    auto now = std::chrono::steady_clock::now();
    f64 current_time = std::chrono::duration<f64, std::milli>(now.time_since_epoch()).count();

    std::vector<Timer> fired;

    // Find fired timers
    for (auto& timer : m_timers) {
        if (current_time >= timer.next_fire_time) {
            fired.push_back(timer);
            if (timer.is_interval) {
                timer.next_fire_time = current_time + timer.interval_ms;
            }
        }
    }

    // Remove one-shot timers
    m_timers.erase(
        std::remove_if(m_timers.begin(), m_timers.end(),
            [current_time](const Timer& t) {
                return !t.is_interval && current_time >= t.next_fire_time;
            }),
        m_timers.end());

    // Execute callbacks
    for (const auto& timer : fired) {
        // Call the callback
        // (In a real implementation, this would properly call the JS function)
    }
}

void TimerBindings::clear_all() {
    m_timers.clear();
}

// ============================================================================
// FetchBindings implementation
// ============================================================================

FetchBindings::FetchBindings(js::VM& vm) : m_vm(vm) {}

void FetchBindings::register_fetch() {
    // fetch() - simplified implementation
    m_vm.define_native("fetch"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Would return a Promise in a full implementation
        return js::Value::undefined();
    });
}

// ============================================================================
// Register All Bindings
// ============================================================================

void register_all_bindings(js::VM& vm, dom::Document* document) {
    DOMBindings dom_bindings(vm);
    if (document) {
        dom_bindings.set_document(RefPtr<dom::Document>(document));
    }
    dom_bindings.register_all();

    ConsoleBindings console_bindings(vm);
    console_bindings.register_console();

    TimerBindings timer_bindings(vm);
    timer_bindings.register_timers();

    FetchBindings fetch_bindings(vm);
    fetch_bindings.register_fetch();
}

} // namespace lithium::bindings
