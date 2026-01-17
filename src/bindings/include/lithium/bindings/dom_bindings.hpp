#pragma once

#include "lithium/js/vm.hpp"
#include "lithium/dom/document.hpp"

namespace lithium::bindings {

// ============================================================================
// DOM Bindings - Expose DOM API to JavaScript
// ============================================================================

class DOMBindings {
public:
    DOMBindings(js::VM& vm);

    // Register all DOM bindings
    void register_all();

    // Set the current document
    void set_document(RefPtr<dom::Document> document);

    // Get the current document
    [[nodiscard]] dom::Document* document() const { return m_document.get(); }

private:
    // Window object
    void register_window();

    // Document object
    void register_document();

    // Element class
    void register_element();

    // Node class
    void register_node();

    // Text class
    void register_text();

    // Event classes
    void register_event();
    void register_event_target();

    // For embedding/tests: wrap a DOM node as a JS value
    [[nodiscard]] js::Value wrap_node_for_script(dom::Node* node);

    // Helper: wrap DOM node as JS object
    [[nodiscard]] js::Value wrap_node(dom::Node* node);

    // Helper: unwrap JS object to DOM node
    [[nodiscard]] dom::Node* unwrap_node(const js::Value& value);

    js::VM& m_vm;
    RefPtr<dom::Document> m_document;

    // Wrapped object cache
    std::unordered_map<dom::Node*, std::shared_ptr<js::Object>> m_node_wrappers;
};

// ============================================================================
// Console Bindings
// ============================================================================

class ConsoleBindings {
public:
    ConsoleBindings(js::VM& vm);

    void register_console();

    using LogCallback = std::function<void(const String& level, const String& message)>;
    void set_log_callback(LogCallback callback) { m_log_callback = std::move(callback); }

private:
    js::VM& m_vm;
    LogCallback m_log_callback;
};

// ============================================================================
// Timer Bindings (setTimeout, setInterval)
// ============================================================================

class TimerBindings {
public:
    TimerBindings(js::VM& vm);

    void register_timers();

    // Process pending timers (call from event loop)
    void process_timers();

    // Clear all timers
    void clear_all();

private:
    struct Timer {
        u32 id;
        js::Value callback;
        f64 interval_ms;
        f64 next_fire_time;
        bool is_interval;
    };

    js::VM& m_vm;
    std::vector<Timer> m_timers;
    u32 m_next_timer_id{1};
};

// ============================================================================
// Fetch API Bindings
// ============================================================================

class FetchBindings {
public:
    FetchBindings(js::VM& vm);

    void register_fetch();

private:
    js::VM& m_vm;
};

// ============================================================================
// Register All Built-in Bindings
// ============================================================================

void register_all_bindings(js::VM& vm, dom::Document* document = nullptr);

} // namespace lithium::bindings
