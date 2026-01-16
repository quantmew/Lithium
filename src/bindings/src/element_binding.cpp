/**
 * Element Binding - JavaScript Element object binding
 */

#include "lithium/bindings/dom_bindings.hpp"
#include "lithium/dom/element.hpp"

namespace lithium::bindings {

// ============================================================================
// Element binding helpers
// ============================================================================

js::Value create_element_wrapper(dom::Element* element, js::VM& vm) {
    if (!element) {
        return js::Value::null();
    }

    auto wrapper = std::make_shared<js::Object>();

    // Element properties
    wrapper->set_property("tagName"_s, js::Value(element->tag_name()));
    wrapper->set_property("id"_s, js::Value(element->id()));
    wrapper->set_property("className"_s, js::Value(element->class_name()));
    wrapper->set_property("innerHTML"_s, js::Value(element->inner_html()));
    wrapper->set_property("outerHTML"_s, js::Value(element->outer_html()));
    wrapper->set_property("textContent"_s, js::Value(element->text_content()));

    // Node properties
    wrapper->set_property("nodeType"_s, js::Value(static_cast<f64>(1)));  // ELEMENT_NODE
    wrapper->set_property("nodeName"_s, js::Value(element->node_name()));

    // Child count
    usize child_count = 0;
    for (auto* child = element->first_child(); child; child = child->next_sibling()) {
        ++child_count;
    }
    wrapper->set_property("childElementCount"_s, js::Value(static_cast<f64>(child_count)));

    return js::Value(wrapper);
}

void register_element_methods(js::VM& vm) {
    // Element methods would be added to element prototype
    // This is a simplified implementation

    // getAttribute
    // setAttribute
    // removeAttribute
    // hasAttribute
    // querySelector
    // querySelectorAll
    // appendChild
    // removeChild
    // insertBefore
    // replaceChild
    // cloneNode
    // addEventListener
    // removeEventListener
    // dispatchEvent
    // etc.
}

// ============================================================================
// Node binding helpers
// ============================================================================

void register_node_methods(js::VM& vm) {
    // Node methods would be added to node prototype
}

// ============================================================================
// EventTarget binding helpers
// ============================================================================

void register_event_target_methods(js::VM& vm) {
    // addEventListener
    vm.define_native("addEventListener"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Would add event listener to target
        return js::Value::undefined();
    });

    // removeEventListener
    vm.define_native("removeEventListener"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Would remove event listener from target
        return js::Value::undefined();
    });

    // dispatchEvent
    vm.define_native("dispatchEvent"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Would dispatch event
        return js::Value(true);
    });
}

} // namespace lithium::bindings
