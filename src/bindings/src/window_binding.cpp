/**
 * Window Binding - JavaScript Window object binding
 */

#include "lithium/bindings/dom_bindings.hpp"

namespace lithium::bindings {

// ============================================================================
// Window binding implementation
// ============================================================================

void register_window_object(js::VM& vm) {
    // window.location
    vm.define_native("location"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Return a location object (stub)
        return js::Value::undefined();
    });

    // window.navigator
    vm.define_native("navigator"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Return a navigator object (stub)
        return js::Value::undefined();
    });

    // window.innerWidth
    vm.set_global("innerWidth"_s, js::Value(800.0), false);

    // window.innerHeight
    vm.set_global("innerHeight"_s, js::Value(600.0), false);

    // window.alert
    vm.define_native("alert"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Alert implementation (stub - would show dialog)
        return js::Value::undefined();
    });

    // window.confirm
    vm.define_native("confirm"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Confirm implementation (stub)
        return js::Value(false);
    });

    // window.prompt
    vm.define_native("prompt"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Prompt implementation (stub)
        return js::Value::null();
    });

    // window.open
    vm.define_native("open"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Open new window (stub)
        return js::Value::null();
    });

    // window.close
    vm.define_native("close"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Close window (stub)
        return js::Value::undefined();
    });

    // window.requestAnimationFrame
    vm.define_native("requestAnimationFrame"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Request animation frame (stub)
        static i32 frame_id = 0;
        return js::Value(static_cast<f64>(++frame_id));
    });

    // window.cancelAnimationFrame
    vm.define_native("cancelAnimationFrame"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Cancel animation frame (stub)
        return js::Value::undefined();
    });

    // window.getComputedStyle
    vm.define_native("getComputedStyle"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Get computed style (stub)
        return js::Value::null();
    });

    // window.scrollTo
    vm.define_native("scrollTo"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Scroll to position (stub)
        return js::Value::undefined();
    });

    // window.scrollBy
    vm.define_native("scrollBy"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        // Scroll by amount (stub)
        return js::Value::undefined();
    });
}

} // namespace lithium::bindings
