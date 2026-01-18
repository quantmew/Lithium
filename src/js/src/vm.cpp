/**
 * JavaScript VM - bytecode interpreter with lexical environments
 *
 * Optimizations:
 * - Computed Goto (Direct Threading) for ~20-30% faster opcode dispatch
 * - Inline Caching for property access
 * - Shape-based hidden classes for objects
 */

#include "lithium/js/vm.hpp"
#include "lithium/js/compiler.hpp"
#include "lithium/js/parser.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

// ============================================================================
// Computed Goto (Direct Threading) Dispatch
// ============================================================================
// GCC/Clang extension for faster opcode dispatch. Each opcode handler ends
// with a direct jump to the next handler, eliminating switch overhead.

#if defined(__GNUC__) || defined(__clang__)
    #define USE_COMPUTED_GOTO 1
#else
    #define USE_COMPUTED_GOTO 0
#endif

namespace lithium::js {

namespace {

class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const String& message)
        : std::runtime_error(message.std_string()) {}
};

class JSException : public std::exception {
public:
    explicit JSException(Value v)
        : m_value(std::move(v)) {}
    [[nodiscard]] const Value& value() const { return m_value; }

private:
    Value m_value;
};

f64 current_time_millis() {
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    return static_cast<f64>(duration_cast<milliseconds>(now).count());
}

} // namespace

struct VMFunctionObject : public Object {
    VMFunctionObject(std::shared_ptr<FunctionCode> fn, std::shared_ptr<VM::Environment> env)
        : function(std::move(fn)), closure(std::move(env)) {}

    [[nodiscard]] bool is_callable() const override { return true; }

    std::shared_ptr<FunctionCode> function;
    std::shared_ptr<VM::Environment> closure;
};

// ============================================================================
// Environment
// ============================================================================

VM::Environment::Environment(std::shared_ptr<Environment> parent, std::shared_ptr<Object> global_object)
    : m_parent(std::move(parent))
{
    if (m_parent) {
        m_global_object = m_parent->m_global_object;
    } else {
        m_global_object = std::move(global_object);
    }
}

VM::Environment::Environment(std::shared_ptr<Environment> parent, Value with_object)
    : m_parent(std::move(parent))
    , m_with_object(std::move(with_object)) {
    if (m_parent) {
        m_global_object = m_parent->m_global_object;
    }
}

void VM::Environment::bind_function(const std::shared_ptr<FunctionCode>& function) {
    m_function = function;
    if (m_function) {
        m_locals.assign(m_function->local_count, Value::undefined());
        m_local_is_const = m_function->local_is_const;
    } else {
        m_locals.clear();
        m_local_is_const.clear();
    }
}

Value VM::Environment::get_local(u16 slot) const {
    if (slot >= m_locals.size()) {
        return Value::undefined();
    }
    return m_locals[slot];
}

bool VM::Environment::set_local(u16 slot, const Value& value) {
    if (slot >= m_locals.size()) {
        return false;
    }
    bool is_const = slot < m_local_is_const.size() ? m_local_is_const[slot] : false;
    if (is_const && !m_locals[slot].is_undefined() && !value.is_undefined()) {
        return false;
    }
    m_locals[slot] = value;
    if (m_is_global && m_global_object && m_function && slot < m_function->local_names.size()) {
        m_global_object->set_property(m_function->local_names[slot], value);
    }
    return true;
}

void VM::Environment::define(const String& name, Value value, bool is_const) {
    if (m_function) {
        if (auto slot = m_function->resolve_local(name)) {
            if (slot.value() >= m_local_is_const.size()) {
                m_local_is_const.resize(slot.value() + 1, false);
            }
            m_local_is_const[slot.value()] = m_local_is_const[slot.value()] || is_const;
            set_local(slot.value(), value);
            return;
        }
    }
    m_values[name] = Binding{std::move(value), is_const};
    if (m_is_global && m_global_object) {
        m_global_object->set_property(name, m_values[name].value);
    }
}

bool VM::Environment::assign(const String& name, const Value& value) {
    if (m_with_object && m_with_object->is_object()) {
        auto* obj = m_with_object->as_object();
        obj->set_property(name, value);
        return true;
    }

    if (m_function) {
        if (auto slot = m_function->resolve_local(name)) {
            return set_local(slot.value(), value);
        }
    }

    auto it = m_values.find(name);
    if (it != m_values.end()) {
        if (it->second.is_const && !it->second.value.is_undefined()) {
            return false;
        }
        it->second.value = value;
        if (m_is_global && m_global_object) {
            m_global_object->set_property(name, value);
        }
        return true;
    }
    if (m_parent) {
        return m_parent->assign(name, value);
    }
    if (m_is_global && m_global_object) {
        m_global_object->set_property(name, value);
        return true;
    }
    return false;
}

std::optional<VM::Binding> VM::Environment::get(const String& name) const {
    if (m_with_object && m_with_object->is_object()) {
        auto* obj = m_with_object->as_object();
        if (obj->has_property(name)) {
            return Binding{obj->get_property(name), false};
        }
    }

    if (m_function) {
        if (auto slot = m_function->resolve_local(name)) {
            Value val = get_local(slot.value());
            bool is_const = slot.value() < m_local_is_const.size() ? m_local_is_const[slot.value()] : false;
            return Binding{val, is_const};
        }
    }

    auto it = m_values.find(name);
    if (it != m_values.end()) {
        return it->second;
    }
    if (m_parent) {
        return m_parent->get(name);
    }
    return std::nullopt;
}

// ============================================================================
// Built-ins
// ============================================================================

void VM::install_global(const String& name, const Value& value, bool is_const) {
    if (m_global_object) {
        m_global_object->set_property(name, value);
    }
    m_global_env->define(name, value, is_const);
}

void VM::set_global(const String& name, const Value& value, bool is_const) {
    install_global(name, value, is_const);
}

void VM::init_builtins() {
    // Object constructor
    define_native("Object"_s, [this](VM& vm, const std::vector<Value>& args) -> Value {
        Value arg = args.empty() ? Value(vm.m_gc.allocate<Object>()) : args[0];
        if (!arg.is_object()) {
            arg = Value(vm.m_gc.allocate<Object>());
        }
        auto* obj = arg.as_object();
        if (obj && !obj->prototype()) {
            obj->set_prototype(vm.m_object_prototype);
        }
        return arg;
    }, 1);

    // Array constructor
    define_native("Array"_s, [this](VM& vm, const std::vector<Value>& args) -> Value {
        auto arr = vm.m_gc.allocate<Array>();
        arr->set_prototype(m_array_prototype);
        for (const auto& v : args) {
            arr->push(v);
        }
        return Value(arr);
    }, 1);

    // Minimal Function constructor placeholder
    define_native("Function"_s, [](VM&, const std::vector<Value>&) -> Value {
        return Value::undefined();
    }, 0);

    auto get_global_binding = [&](const String& name) -> Value {
        auto binding = m_global_env->get(name);
        if (binding.has_value()) {
            return binding->value;
        }
        return Value::undefined();
    };

    // Wire constructor prototypes
    Value obj_ctor = get_global_binding("Object"_s);
    if (obj_ctor.is_object()) {
        obj_ctor.as_object()->set_property("prototype"_s, Value(m_object_prototype));
        m_object_prototype->set_property("constructor"_s, obj_ctor);
    }
    Value arr_ctor = get_global_binding("Array"_s);
    if (arr_ctor.is_object()) {
        arr_ctor.as_object()->set_property("prototype"_s, Value(m_array_prototype));
        m_array_prototype->set_property("constructor"_s, arr_ctor);
    }

    // Array prototype methods
    auto array_push = [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* arr = receiver.as<Array>();
        if (!arr) {
            return Value::undefined();
        }
        for (const auto& v : args) {
            arr->push(v);
        }
        return Value(static_cast<f64>(arr->length()));
    };

    auto array_pop = [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* arr = receiver.as<Array>();
        if (!arr) {
            return Value::undefined();
        }
        return arr->pop();
    };

    auto array_join = [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* arr = receiver.as<Array>();
        if (!arr) {
            return Value::undefined();
        }
        String sep = args.empty() ? ","_s : args[0].to_string();
        std::ostringstream oss;
        for (usize i = 0; i < arr->length(); ++i) {
            if (i > 0) {
                oss << sep.std_string();
            }
            oss << arr->get_element(static_cast<u32>(i)).to_string().std_string();
        }
        return String(oss.str());
    };

    auto make_fn = [&](const String& nm, NativeFn fn, u8 arity) {
        auto nf = m_gc.allocate<NativeFunction>(nm, fn, arity);
        nf->set_prototype(m_function_prototype);
        nf->set_property("length"_s, Value(static_cast<f64>(arity)));
        nf->set_property("name"_s, Value(nm));
        return Value(nf);
    };

    m_array_prototype->set_property("push"_s, make_fn("push"_s, array_push, 1));
    m_array_prototype->set_property("pop"_s, make_fn("pop"_s, array_pop, 0));
    m_array_prototype->set_property("join"_s, make_fn("join"_s, array_join, 1));

    // Object prototype methods
    auto has_own = [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        if (!receiver.is_object() || args.empty()) {
            return Value(false);
        }
        return Value(receiver.as_object()->has_own_property(args[0].to_string()));
    };
    m_object_prototype->set_property("hasOwnProperty"_s, make_fn("hasOwnProperty"_s, has_own, 1));

    // Math object
    auto math = m_gc.allocate<Object>();
    math->set_prototype(m_object_prototype);
    math->set_property("abs"_s, make_fn("abs"_s, [](VM&, const std::vector<Value>& args) -> Value {
        f64 v = args.empty() ? 0.0 : args[0].to_number();
        return Value(std::fabs(v));
    }, 1));
    math->set_property("floor"_s, make_fn("floor"_s, [](VM&, const std::vector<Value>& args) -> Value {
        f64 v = args.empty() ? 0.0 : args[0].to_number();
        return Value(std::floor(v));
    }, 1));
    math->set_property("ceil"_s, make_fn("ceil"_s, [](VM&, const std::vector<Value>& args) -> Value {
        f64 v = args.empty() ? 0.0 : args[0].to_number();
        return Value(std::ceil(v));
    }, 1));
    math->set_property("max"_s, make_fn("max"_s, [](VM&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(-std::numeric_limits<f64>::infinity());
        f64 best = args[0].to_number();
        for (usize i = 1; i < args.size(); ++i) {
            best = std::max(best, args[i].to_number());
        }
        return Value(best);
    }, 1));
    math->set_property("min"_s, make_fn("min"_s, [](VM&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(std::numeric_limits<f64>::infinity());
        f64 best = args[0].to_number();
        for (usize i = 1; i < args.size(); ++i) {
            best = std::min(best, args[i].to_number());
        }
        return Value(best);
    }, 1));
    math->set_property("random"_s, make_fn("random"_s, [](VM&, const std::vector<Value>&) -> Value {
        return Value(static_cast<f64>(std::rand()) / static_cast<f64>(RAND_MAX));
    }, 0));
    math->set_property("sqrt"_s, make_fn("sqrt"_s, [](VM&, const std::vector<Value>& args) -> Value {
        f64 v = args.empty() ? 0.0 : args[0].to_number();
        return Value(std::sqrt(v));
    }, 1));
    math->set_property("PI"_s, Value(3.141592653589793));
    install_global("Math"_s, Value(math), true);

    // JSON object
    auto json = m_gc.allocate<Object>();
    json->set_prototype(m_object_prototype);
    json->set_property("stringify"_s, make_fn("stringify"_s, [](VM&, const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].is_undefined()) {
            return Value("undefined"_s);
        }
        return args[0].to_string();
    }, 1));
    install_global("JSON"_s, Value(json), true);

    // Date object (minimal - time value and basic stringification)
    auto date_proto = m_gc.allocate<DateObject>();
    date_proto->set_prototype(m_object_prototype);

    auto date_ctor = make_fn("Date"_s, [this, date_proto](VM&, const std::vector<Value>& args) -> Value {
        f64 timestamp = args.empty() ? current_time_millis() : args[0].to_number();
        auto date = m_gc.allocate<DateObject>(timestamp);
        date->set_prototype(date_proto);
        return Value(date);
    }, 1);

    date_proto->set_property("valueOf"_s, make_fn("valueOf"_s, [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* date = receiver.as<DateObject>();
        if (!date) {
            return Value::undefined();
        }
        return Value(date->time_value());
    }, 0));

    date_proto->set_property("getTime"_s, make_fn("getTime"_s, [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* date = receiver.as<DateObject>();
        if (!date) {
            return Value::undefined();
        }
        return Value(date->time_value());
    }, 0));

    date_proto->set_property("toString"_s, make_fn("toString"_s, [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* date = receiver.as<DateObject>();
        if (!date) {
            return Value("Invalid Date"_s);
        }
        return Value(date->string_value());
    }, 0));

    if (auto* ctor_obj = date_ctor.as_object()) {
        ctor_obj->set_property("prototype"_s, Value(date_proto));
        ctor_obj->set_property("now"_s, make_fn("now"_s, [](VM&, const std::vector<Value>&) -> Value {
            return Value(current_time_millis());
        }, 0));
    }
    date_proto->set_property("constructor"_s, date_ctor);
    install_global("Date"_s, date_ctor, true);

    // Map constructor and prototype
    auto map_proto = m_gc.allocate<MapObject>();
    map_proto->set_prototype(m_object_prototype);

    auto map_ctor = make_fn("Map"_s, [this, map_proto](VM&, const std::vector<Value>&) -> Value {
        auto map = m_gc.allocate<MapObject>();
        map->set_prototype(map_proto);
        return Value(map);
    }, 0);

    map_proto->set_property("set"_s, make_fn("set"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* map = receiver.as<MapObject>();
        if (!map) return Value::undefined();
        if (args.size() < 2) return receiver;
        map->set(args[0], args[1]);
        return receiver;
    }, 2));

    map_proto->set_property("get"_s, make_fn("get"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* map = receiver.as<MapObject>();
        if (!map || args.empty()) return Value::undefined();
        return map->get(args[0]);
    }, 1));

    map_proto->set_property("has"_s, make_fn("has"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* map = receiver.as<MapObject>();
        if (!map || args.empty()) return Value(false);
        return Value(map->has(args[0]));
    }, 1));

    map_proto->set_property("delete"_s, make_fn("delete"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* map = receiver.as<MapObject>();
        if (!map || args.empty()) return Value(false);
        return Value(map->remove(args[0]));
    }, 1));

    map_proto->set_property("clear"_s, make_fn("clear"_s, [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* map = receiver.as<MapObject>();
        if (map) map->clear();
        return Value::undefined();
    }, 0));

    // Map.prototype.size getter
    map_proto->set_property("size"_s, Value(0.0));  // Placeholder - will be overridden dynamically

    if (auto* ctor_obj = map_ctor.as_object()) {
        ctor_obj->set_property("prototype"_s, Value(map_proto));
    }
    map_proto->set_property("constructor"_s, map_ctor);
    install_global("Map"_s, map_ctor, true);

    // Set constructor and prototype
    auto set_proto = m_gc.allocate<SetObject>();
    set_proto->set_prototype(m_object_prototype);

    auto set_ctor = make_fn("Set"_s, [this, set_proto](VM&, const std::vector<Value>&) -> Value {
        auto set = m_gc.allocate<SetObject>();
        set->set_prototype(set_proto);
        return Value(set);
    }, 0);

    set_proto->set_property("add"_s, make_fn("add"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* set = receiver.as<SetObject>();
        if (!set || args.empty()) return receiver;
        set->add(args[0]);
        return receiver;
    }, 1));

    set_proto->set_property("has"_s, make_fn("has"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* set = receiver.as<SetObject>();
        if (!set || args.empty()) return Value(false);
        return Value(set->has(args[0]));
    }, 1));

    set_proto->set_property("delete"_s, make_fn("delete"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* set = receiver.as<SetObject>();
        if (!set || args.empty()) return Value(false);
        return Value(set->remove(args[0]));
    }, 1));

    set_proto->set_property("clear"_s, make_fn("clear"_s, [](VM& vm, const std::vector<Value>&) -> Value {
        auto receiver = vm.current_this();
        auto* set = receiver.as<SetObject>();
        if (set) set->clear();
        return Value::undefined();
    }, 0));

    // Set.prototype.size getter
    set_proto->set_property("size"_s, Value(0.0));  // Placeholder - will be overridden dynamically

    if (auto* ctor_obj = set_ctor.as_object()) {
        ctor_obj->set_property("prototype"_s, Value(set_proto));
    }
    set_proto->set_property("constructor"_s, set_ctor);
    install_global("Set"_s, set_ctor, true);

    // WeakMap constructor and prototype
    auto weakmap_proto = m_gc.allocate<WeakMapObject>();
    weakmap_proto->set_prototype(m_object_prototype);

    auto weakmap_ctor = make_fn("WeakMap"_s, [this, weakmap_proto](VM&, const std::vector<Value>&) -> Value {
        auto weakmap = m_gc.allocate<WeakMapObject>();
        weakmap->set_prototype(weakmap_proto);
        return Value(weakmap);
    }, 0);

    weakmap_proto->set_property("set"_s, make_fn("set"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakmap = receiver.as<WeakMapObject>();
        if (!weakmap || args.size() < 2) return receiver;
        weakmap->set(args[0], args[1]);
        return receiver;
    }, 2));

    weakmap_proto->set_property("get"_s, make_fn("get"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakmap = receiver.as<WeakMapObject>();
        if (!weakmap || args.empty()) return Value::undefined();
        return weakmap->get(args[0]);
    }, 1));

    weakmap_proto->set_property("has"_s, make_fn("has"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakmap = receiver.as<WeakMapObject>();
        if (!weakmap || args.empty()) return Value(false);
        return Value(weakmap->has(args[0]));
    }, 1));

    weakmap_proto->set_property("delete"_s, make_fn("delete"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakmap = receiver.as<WeakMapObject>();
        if (!weakmap || args.empty()) return Value(false);
        return Value(weakmap->remove(args[0]));
    }, 1));

    if (auto* ctor_obj = weakmap_ctor.as_object()) {
        ctor_obj->set_property("prototype"_s, Value(weakmap_proto));
    }
    weakmap_proto->set_property("constructor"_s, weakmap_ctor);
    install_global("WeakMap"_s, weakmap_ctor, true);

    // WeakSet constructor and prototype
    auto weakset_proto = m_gc.allocate<WeakSetObject>();
    weakset_proto->set_prototype(m_object_prototype);

    auto weakset_ctor = make_fn("WeakSet"_s, [this, weakset_proto](VM&, const std::vector<Value>&) -> Value {
        auto weakset = m_gc.allocate<WeakSetObject>();
        weakset->set_prototype(weakset_proto);
        return Value(weakset);
    }, 0);

    weakset_proto->set_property("add"_s, make_fn("add"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakset = receiver.as<WeakSetObject>();
        if (!weakset || args.empty()) return receiver;
        weakset->add(args[0]);
        return receiver;
    }, 1));

    weakset_proto->set_property("has"_s, make_fn("has"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakset = receiver.as<WeakSetObject>();
        if (!weakset || args.empty()) return Value(false);
        return Value(weakset->has(args[0]));
    }, 1));

    weakset_proto->set_property("delete"_s, make_fn("delete"_s, [](VM& vm, const std::vector<Value>& args) -> Value {
        auto receiver = vm.current_this();
        auto* weakset = receiver.as<WeakSetObject>();
        if (!weakset || args.empty()) return Value(false);
        return Value(weakset->remove(args[0]));
    }, 1));

    if (auto* ctor_obj = weakset_ctor.as_object()) {
        ctor_obj->set_property("prototype"_s, Value(weakset_proto));
    }
    weakset_proto->set_property("constructor"_s, weakset_ctor);
    install_global("WeakSet"_s, weakset_ctor, true);

    // Basic globals
    install_global("globalThis"_s, Value(m_global_object), true);
}

// ============================================================================
// VM
// ============================================================================

VM::VM() {
    // Allocate prototype objects through GC
    m_object_prototype = m_gc.allocate<Object>();
    m_function_prototype = m_gc.allocate<Object>();
    m_function_prototype->set_prototype(m_object_prototype);
    m_array_prototype = m_gc.allocate<Array>();
    m_array_prototype->set_prototype(m_object_prototype);
    m_global_object = m_gc.allocate<Object>();
    m_global_object->set_prototype(m_object_prototype);

    m_global_env = std::make_shared<Environment>(nullptr, m_global_object);
    m_global_env->m_is_global = true;
    init_builtins();
}

void VM::define_native(const String& name, NativeFn fn, u8 arity) {
    auto native_obj = m_gc.allocate<NativeFunction>(name, std::move(fn), arity);
    native_obj->set_prototype(m_function_prototype);
    native_obj->set_property("length"_s, Value(static_cast<f64>(arity)));
    native_obj->set_property("name"_s, Value(name));
    install_global(name, Value(native_obj), true);
}

void VM::push(const Value& value) {
    m_stack.push_back(value);
}

Value VM::pop() {
    if (m_stack.empty()) {
        return Value::undefined();
    }
    Value v = m_stack.back();
    m_stack.pop_back();
    return v;
}

Value& VM::peek(usize distance) {
    return m_stack[m_stack.size() - 1 - distance];
}

const Value& VM::peek(usize distance) const {
    return m_stack[m_stack.size() - 1 - distance];
}

Value VM::read_constant(const CallFrame& frame, u16 idx) const {
    return frame.function->chunk.constants()[idx];
}

VM::InterpretResult VM::interpret(const String& source, const String& filename) {
    m_diagnostics.clear();
    m_source_code = source;  // Store source code for error reporting
    m_source_file = filename;  // File name for error reporting

    Parser parser;
    parser.set_diagnostics(&m_diagnostics);
    auto program = parser.parse(source);
    if (parser.has_errors()) {
        const auto& diag = m_diagnostics.diagnostics().empty() ? parser.errors().front() : m_diagnostics.diagnostics().front().message;
        m_error_message = diag;
        return InterpretResult::ParseError;
    }

    Compiler compiler;
    auto result = compiler.compile(*program);
    if (!result.errors.empty()) {
        m_error_message = result.errors.front();
        m_diagnostics.add(DiagnosticStage::Compiler, DiagnosticLevel::Error, result.errors.front());
        return InterpretResult::ParseError;
    }

    m_module = std::move(result.module);
    m_frames.clear();
    m_stack.clear();
    m_handlers.clear();

    auto entry_fn = m_module.functions[m_module.entry];
    m_global_env->bind_function(entry_fn);
    auto frame_env = m_global_env;
    m_frames.push_back(CallFrame{entry_fn, frame_env, frame_env, 0, 0, Value::undefined(), {}});
    m_frames.back().init_ic_cache();

    // GC check counter (check every N instructions)
    usize gc_check_counter = 0;
    constexpr usize GC_CHECK_INTERVAL = 100;

    try {
#if USE_COMPUTED_GOTO
        // ====================================================================
        // Computed Goto Dispatch Table
        // ====================================================================
        // Direct threading: each handler jumps directly to the next handler
        // without going through a central dispatch point.
        static void* dispatch_table[] = {
            &&op_LoadConst, &&op_LoadNull, &&op_LoadUndefined, &&op_LoadTrue,
            &&op_LoadFalse, &&op_Pop, &&op_Dup, &&op_Dup2, &&op_DefineVar,
            &&op_GetVar, &&op_SetVar, &&op_GetLocal, &&op_SetLocal, &&op_GetProp,
            &&op_SetProp, &&op_GetElem, &&op_SetElem, &&op_GetPropIC, &&op_SetPropIC,
            &&op_Add, &&op_Subtract, &&op_Multiply, &&op_Divide, &&op_Modulo,
            &&op_Exponent, &&op_LeftShift, &&op_RightShift, &&op_UnsignedRightShift,
            &&op_BitwiseAnd, &&op_BitwiseOr, &&op_BitwiseXor, &&op_BitwiseNot,
            &&op_Negate, &&op_StrictEqual, &&op_StrictNotEqual, &&op_LessThan,
            &&op_LessEqual, &&op_GreaterThan, &&op_GreaterEqual, &&op_Instanceof,
            &&op_In, &&op_Typeof, &&op_Void, &&op_LogicalNot, &&op_Jump,
            &&op_JumpIfFalse, &&op_JumpIfNullish, &&op_Throw, &&op_PushHandler,
            &&op_PopHandler, &&op_MakeArray, &&op_ArrayPush, &&op_ArraySpread,
            &&op_MakeObject, &&op_ObjectSpread, &&op_GetOwnPropertyNames, &&op_MakeFunction, &&op_Call,
            &&op_New, &&op_NewStack, &&op_Return, &&op_This, &&op_EnterWith,
            &&op_ExitWith,
        };

        // Note: We use a pointer because references cannot be reseated when
        // frames change (Call/Return). The pointer is updated on each dispatch.
        CallFrame* frame_ptr = &m_frames.back();
        #define frame (*frame_ptr)

        // ====================================================================
        // Inline Stack Operations (avoid function call overhead)
        // ====================================================================
        #define VM_PUSH(val) m_stack.push_back(val)
        #define VM_POP() ([this]() -> Value { \
            Value v = std::move(m_stack.back()); \
            m_stack.pop_back(); \
            return v; \
        })()
        #define VM_PEEK(dist) m_stack[m_stack.size() - 1 - (dist)]
        #define VM_PEEK_REF(dist) m_stack[m_stack.size() - 1 - (dist)]

        // ====================================================================
        // Inline Local Variable Access (fast path for non-with environments)
        // ====================================================================
        #define VM_GET_LOCAL_FAST(slot) frame.lexical_env->m_locals[slot]
        #define VM_SET_LOCAL_FAST(slot, val) (frame.lexical_env->m_locals[slot] = (val))

        // ====================================================================
        // Fast Number Arithmetic (avoid Value copy overhead)
        // ====================================================================
        // Check if a value is a direct double (most common case for numbers)
        #define VM_IS_NUMBER(v) ((v).is_number())
        #define VM_AS_NUMBER(v) ((v).as_number())

        #define VM_DISPATCH() do { \
            if (m_frames.empty()) goto vm_exit; \
            if (++gc_check_counter >= GC_CHECK_INTERVAL) { \
                gc_check_counter = 0; \
                if (m_gc.should_collect()) { \
                    m_gc.collect(*this); \
                } \
            } \
            frame_ptr = &m_frames.back(); \
            if (frame.ip >= frame.function->chunk.size()) \
                throw RuntimeError("Instruction pointer out of range"_s); \
            goto *dispatch_table[frame.function->chunk.read(frame.ip++)]; \
        } while(0)

        #define VM_CASE(name) op_##name
        #define VM_NEXT() VM_DISPATCH()

        // Initial dispatch
        goto *dispatch_table[frame.function->chunk.read(frame.ip++)];

#else
        // ====================================================================
        // Switch-Case Dispatch (fallback for non-GCC/Clang compilers)
        // ====================================================================
        #define VM_CASE(name) case OpCode::name
        #define VM_NEXT() break

        while (!m_frames.empty()) {
            // Periodic GC check
            if (++gc_check_counter >= GC_CHECK_INTERVAL) {
                gc_check_counter = 0;
                if (m_gc.should_collect()) {
                    m_gc.collect(*this);
                }
            }

            auto& frame = m_frames.back();
            if (frame.ip >= frame.function->chunk.size()) {
                throw RuntimeError("Instruction pointer out of range"_s);
            }
            OpCode op = static_cast<OpCode>(frame.function->chunk.read(frame.ip++));
            switch (op) {
#endif

                VM_CASE(LoadConst): {
                    u16 idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    push(read_constant(frame, idx));
                    VM_NEXT();
                }
                VM_CASE(LoadNull):
                    push(Value(nullptr));
                    VM_NEXT();
                VM_CASE(LoadUndefined):
                    push(Value::undefined());
                    VM_NEXT();
                VM_CASE(LoadTrue):
                    push(Value(true));
                    VM_NEXT();
                VM_CASE(LoadFalse):
                    push(Value(false));
                    VM_NEXT();
                VM_CASE(Pop):
                    pop();
                    VM_NEXT();
                VM_CASE(Dup):
                    push(peek());
                    VM_NEXT();
                VM_CASE(Dup2): {
                    // Duplicate top two values: [a, b] -> [a, b, a, b]
                    Value b = peek(0);
                    Value a = peek(1);
                    push(a);
                    push(b);
                    VM_NEXT();
                }
                VM_CASE(DefineVar): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    bool is_const = frame.function->chunk.read(frame.ip++) != 0;
                    auto name = read_constant(frame, name_idx).to_string();
                    frame.env->define(name, Value::undefined(), is_const);
                    VM_NEXT();
                }
                VM_CASE(GetVar): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    auto binding = frame.env->get(name);
                    if (!binding) {
                        runtime_error(ErrorType::ReferenceError, name + " is not defined"_s);
                    }
                    push(binding->value);
                    VM_NEXT();
                }
                VM_CASE(SetVar): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value val = pop();
                    if (!frame.env->assign(name, val)) {
                        runtime_error("Assignment to undeclared or const variable: "_s + name);
                    }
                    push(val);
                    VM_NEXT();
                }
                VM_CASE(GetLocal): {
                    u16 slot = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    // Fast path: direct array access (most common case)
                    if (!frame.env->is_with_env()) {
                        VM_PUSH(VM_GET_LOCAL_FAST(slot));
                    } else {
                        // Slow path: with-env requires name lookup
                        const auto& name = frame.function->local_names[slot];
                        auto binding = frame.env->get(name);
                        if (!binding) {
                            runtime_error(ErrorType::ReferenceError, name + " is not defined"_s);
                        }
                        VM_PUSH(binding->value);
                    }
                    VM_NEXT();
                }
                VM_CASE(SetLocal): {
                    u16 slot = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    Value val = VM_POP();
                    // Fast path: direct array access (most common case)
                    if (!frame.env->is_with_env()) {
                        // Check if the local is const before assigning (allow initial assignment)
                        bool is_const = slot < frame.lexical_env->m_local_is_const.size() &&
                                       frame.lexical_env->m_local_is_const[slot];
                        if (is_const && slot < frame.lexical_env->m_locals.size() &&
                            !frame.lexical_env->m_locals[slot].is_undefined() && !val.is_undefined()) {
                            runtime_error("Assignment to constant variable"_s);
                        }
                        VM_SET_LOCAL_FAST(slot, val);
                    } else {
                        // Slow path: with-env requires name lookup
                        const auto& name = frame.function->local_names[slot];
                        if (!frame.env->assign(name, val)) {
                            runtime_error("Assignment to undeclared or const variable: "_s + name);
                        }
                    }
                    VM_PUSH(val);
                    VM_NEXT();
                }
                VM_CASE(GetProp): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error(ErrorType::TypeError, "Cannot read properties of "_s + obj.debug_string());
                    }
                    Value prop = obj.as_object()->get_property(name);
                    if (prop.is_callable()) {
                        prop = Value(m_gc.allocate<BoundFunction>(prop, obj));
                    }
                    push(prop);
                    VM_NEXT();
                }
                VM_CASE(SetProp): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value val = pop();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error("Cannot set property of non-object"_s);
                    }
                    obj.as_object()->set_property(name, val);
                    push(val);
                    VM_NEXT();
                }
                VM_CASE(GetPropIC): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    u16 cache_slot = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error(ErrorType::TypeError, "Cannot read properties of "_s + obj.debug_string());
                    }
                    // Use inline cache for fast property access
                    InlineCacheEntry& cache = frame.ic_cache[cache_slot];
                    Value prop = obj.as_object()->get_property_cached(name, cache);
                    if (prop.is_callable()) {
                        prop = Value(m_gc.allocate<BoundFunction>(prop, obj));
                    }
                    push(prop);
                    VM_NEXT();
                }
                VM_CASE(SetPropIC): {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    u16 cache_slot = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value val = pop();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error("Cannot set property of non-object"_s);
                    }
                    // Use inline cache for fast property access
                    InlineCacheEntry& cache = frame.ic_cache[cache_slot];
                    obj.as_object()->set_property_cached(name, val, cache);
                    push(val);
                    VM_NEXT();
                }
                VM_CASE(GetElem): {
                    Value key = pop();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error("Cannot get element of non-object"_s);
                    }
                    Value prop;
                    if (obj.is_array()) {
                        prop = obj.as<Array>()->get_element(static_cast<u32>(key.to_uint32()));
                    } else {
                        prop = obj.as_object()->get_property(key.to_string());
                    }
                    if (prop.is_callable()) {
                        prop = Value(m_gc.allocate<BoundFunction>(prop, obj));
                    }
                    push(prop);
                    VM_NEXT();
                }
                VM_CASE(SetElem): {
                    Value val = pop();
                    Value key = pop();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error("Cannot set element of non-object"_s);
                    }
                    if (obj.is_array()) {
                        obj.as<Array>()->set_element(static_cast<u32>(key.to_uint32()), val);
                    } else {
                        obj.as_object()->set_property(key.to_string(), val);
                    }
                    push(val);
                    VM_NEXT();
                }
                VM_CASE(Add): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    // Fast path: both numbers - do direct arithmetic
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        f64 result = VM_AS_NUMBER(a) + VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        // Slow path: string concatenation or type coercion
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::add(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(Subtract): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    // Fast path: both numbers
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        f64 result = VM_AS_NUMBER(a) - VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::subtract(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(Multiply): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    // Fast path: both numbers
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        f64 result = VM_AS_NUMBER(a) * VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::multiply(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(Divide): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    // Fast path: both numbers
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        f64 result = VM_AS_NUMBER(a) / VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::divide(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(Modulo): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::modulo(a, b));
                    VM_NEXT();
                }
                VM_CASE(Exponent): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::exponent(a, b));
                    VM_NEXT();
                }
                VM_CASE(LeftShift): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::left_shift(a, b));
                    VM_NEXT();
                }
                VM_CASE(RightShift): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::right_shift(a, b));
                    VM_NEXT();
                }
                VM_CASE(UnsignedRightShift): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::unsigned_right_shift(a, b));
                    VM_NEXT();
                }
                VM_CASE(BitwiseAnd): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_and(a, b));
                    VM_NEXT();
                }
                VM_CASE(BitwiseOr): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_or(a, b));
                    VM_NEXT();
                }
                VM_CASE(BitwiseXor): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_xor(a, b));
                    VM_NEXT();
                }
                VM_CASE(BitwiseNot): {
                    Value v = pop();
                    push(value_ops::bitwise_not(v));
                    VM_NEXT();
                }
                VM_CASE(Negate): {
                    Value v = pop();
                    push(value_ops::negate(v));
                    VM_NEXT();
                }
                VM_CASE(StrictEqual): {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.strict_equals(b)));
                    VM_NEXT();
                }
                VM_CASE(StrictNotEqual): {
                    Value b = pop();
                    Value a = pop();
                    push(Value(!a.strict_equals(b)));
                    VM_NEXT();
                }
                VM_CASE(LessThan): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    // Fast path: both numbers
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        bool result = VM_AS_NUMBER(a) < VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::less_than(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(LessEqual): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        bool result = VM_AS_NUMBER(a) <= VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::less_equal(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(GreaterThan): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        bool result = VM_AS_NUMBER(a) > VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::greater_than(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(GreaterEqual): {
                    Value& b = VM_PEEK_REF(0);
                    Value& a = VM_PEEK_REF(1);
                    if (VM_IS_NUMBER(a) && VM_IS_NUMBER(b)) {
                        bool result = VM_AS_NUMBER(a) >= VM_AS_NUMBER(b);
                        m_stack.pop_back();
                        VM_PEEK_REF(0) = Value(result);
                    } else {
                        Value bv = VM_POP();
                        Value av = VM_POP();
                        VM_PUSH(value_ops::greater_equal(av, bv));
                    }
                    VM_NEXT();
                }
                VM_CASE(Instanceof): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::instanceof_op(a, b));
                    VM_NEXT();
                }
                VM_CASE(In): {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::in_op(a, b));
                    VM_NEXT();
                }
                VM_CASE(LogicalNot): {
                    Value v = pop();
                    push(Value(!v.to_boolean()));
                    VM_NEXT();
                }
                VM_CASE(Typeof): {
                    Value v = pop();
                    push(value_ops::typeof_op(v));
                    VM_NEXT();
                }
                VM_CASE(Void): {
                    push(Value::undefined());
                    VM_NEXT();
                }
                VM_CASE(Jump): {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + 2 + offset);
                    VM_NEXT();
                }
                VM_CASE(JumpIfFalse): {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip += 2;
                    if (!peek().to_boolean()) {
                        frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + offset);
                    }
                    VM_NEXT();
                }
                VM_CASE(JumpIfNullish): {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip += 2;
                    if (peek().is_nullish()) {
                        frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + offset);
                    }
                    VM_NEXT();
                }
                VM_CASE(Throw): {
                    Value v = pop();
                    handle_exception(v);
                    VM_NEXT();
                }
                VM_CASE(PushHandler): {
                    u16 catch_ip = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    u16 finally_ip = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    bool has_catch = frame.function->chunk.read(frame.ip++) != 0;
                    m_handlers.push_back(ExceptionHandler{m_frames.size() - 1, catch_ip, finally_ip, has_catch});
                    VM_NEXT();
                }
                VM_CASE(PopHandler): {
                    if (!m_handlers.empty()) {
                        m_handlers.pop_back();
                    }
                    VM_NEXT();
                }
                VM_CASE(MakeArray): {
                    u16 count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto arr = m_gc.allocate<Array>();
                    arr->set_prototype(m_array_prototype);
                    std::vector<Value> elems;
                    elems.reserve(count);
                    for (u16 i = 0; i < count; ++i) {
                        elems.push_back(pop());
                    }
                    // elems are reversed
                    for (i32 i = static_cast<i32>(count) - 1; i >= 0; --i) {
                        arr->set_element(static_cast<u32>(count - 1 - i), elems[static_cast<usize>(i)]);
                    }
                    push(Value(arr));
                    VM_NEXT();
                }
                VM_CASE(ArrayPush): {
                    Value value = pop();
                    Value arr = pop();
                    if (!arr.is_array()) {
                        runtime_error("Cannot push into non-array"_s);
                    }
                    arr.as<Array>()->push(value);
                    push(arr);
                    VM_NEXT();
                }
                VM_CASE(ArraySpread): {
                    Value source = pop();
                    Value arr = pop();
                    if (!arr.is_array()) {
                        runtime_error("Cannot spread into non-array"_s);
                    }
                    auto* target = arr.as<Array>();
                    if (source.is_array()) {
                        auto* src = source.as<Array>();
                        for (usize i = 0; i < src->length(); ++i) {
                            target->push(src->get_element(static_cast<u32>(i)));
                        }
                    } else if (!source.is_undefined() && !source.is_null()) {
                        target->push(source);
                    }
                    push(arr);
                    VM_NEXT();
                }
                VM_CASE(MakeObject): {
                    auto obj = m_gc.allocate<Object>();
                    obj->set_prototype(m_object_prototype);
                    push(Value(obj));
                    VM_NEXT();
                }
                VM_CASE(ObjectSpread): {
                    Value source = pop();
                    Value target = pop();
                    if (!target.is_object()) {
                        runtime_error("Cannot spread into non-object"_s);
                    }
                    if (source.is_object()) {
                        auto* src = source.as_object();
                        auto* dst = target.as_object();
                        for (const auto& name : src->own_property_names()) {
                            dst->set_property(name, src->get_property(name));
                        }
                    }
                    push(target);
                    VM_NEXT();
                }
                VM_CASE(GetOwnPropertyNames): {
                    // 从栈顶获取对象，返回其所有可枚举属性名的数组
                    Value obj_val = pop();
                    auto arr = m_gc.allocate<Array>();
                    arr->set_prototype(m_array_prototype);

                    if (obj_val.is_object()) {
                        auto* obj = obj_val.as_object();
                        auto prop_names = obj->own_property_names();
                        u32 index = 0;
                        for (const auto& name : prop_names) {
                            arr->set_element(index++, Value(name));
                        }
                    }
                    // 如果不是对象（null, undefined, 原始值等），返回空数组

                    push(Value(arr));
                    VM_NEXT();
                }
                VM_CASE(MakeFunction): {
                    u16 func_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    if (func_idx >= m_module.functions.size()) {
                        runtime_error("Invalid function index"_s);
                    }
                    auto fn_obj = m_gc.allocate<VMFunctionObject>(m_module.functions[func_idx], frame.env);
                    fn_obj->set_prototype(m_function_prototype);
                    auto proto_obj = m_gc.allocate<Object>();
                    proto_obj->set_prototype(m_object_prototype);
                    proto_obj->set_property("constructor"_s, Value(fn_obj));
                    fn_obj->set_property("prototype"_s, Value(proto_obj));
                    fn_obj->set_property("length"_s, Value(static_cast<f64>(fn_obj->function->params.size())));
                    fn_obj->set_property("name"_s, Value(fn_obj->function->name));
                    push(Value(std::static_pointer_cast<Object>(fn_obj)));
                    VM_NEXT();
                }
                VM_CASE(Call): {
                    u16 arg_count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    Value callee = peek(arg_count);
                    Value receiver = Value::undefined();
                    if (auto* bound = dynamic_cast<BoundFunction*>(callee.as_object())) {
                        receiver = bound->receiver();
                        callee = bound->target();
                    }
                    if (auto* native = callee.as_native_function()) {
                        std::vector<Value> args;
                        args.reserve(arg_count);
                        for (usize i = 0; i < arg_count; ++i) {
                            args.push_back(peek(arg_count - 1 - i));
                        }
                        for (usize i = 0; i < arg_count + 1; ++i) {
                            pop();
                        }
                        m_this_stack.push_back(receiver);
                        Value res = native->call(*this, args);
                        m_this_stack.pop_back();
                        push(res);
                    } else if (auto* func_obj = dynamic_cast<VMFunctionObject*>(callee.as_object())) {
                        call_function(func_obj, arg_count, receiver);
                    } else {
                        runtime_error(ErrorType::TypeError, callee.debug_string() + " is not a function"_s);
                    }
                    VM_NEXT();
                }
                VM_CASE(New):
                VM_CASE(NewStack): {
                    bool use_stack_alloc = (static_cast<OpCode>(frame.function->chunk.read(frame.ip - 1)) == OpCode::NewStack);
                    u16 arg_count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    Value callee = peek(arg_count);
                    if (!callee.is_callable()) {
                        runtime_error("Attempted to construct a non-function"_s);
                    }
                    // Create a new object with the constructor's prototype
                    // For NewStack, mark as stack-allocated for potential future pooling
                    auto new_obj = m_gc.allocate<Object>();
                    if (use_stack_alloc) {
                        new_obj->set_stack_allocated(true);
                    }
                    new_obj->set_prototype(m_object_prototype);
                    if (callee.is_object()) {
                        Value proto_val = callee.as_object()->get_property("prototype"_s);
                        if (proto_val.is_object()) {
                            new_obj->set_prototype(std::shared_ptr<Object>(proto_val.as_object(), [](Object*) {}));
                        }
                    }
                    Value receiver(new_obj);
                    if (auto* native = callee.as_native_function()) {
                        std::vector<Value> args;
                        args.reserve(arg_count);
                        for (usize i = 0; i < arg_count; ++i) {
                            args.push_back(peek(arg_count - 1 - i));
                        }
                        for (usize i = 0; i < arg_count + 1; ++i) {
                            pop();
                        }
                        m_this_stack.push_back(receiver);
                        Value res = native->call(*this, args);
                        m_this_stack.pop_back();
                        // If the constructor returned an object, use that; otherwise return new_obj
                        if (res.is_object()) {
                            push(res);
                        } else {
                            push(receiver);
                        }
                    } else if (auto* func_obj = dynamic_cast<VMFunctionObject*>(callee.as_object())) {
                        // For VM functions, we need to call it with the new object as `this`
                        // and return the new object if the function doesn't return an object
                        call_function(func_obj, arg_count, receiver);
                        // Mark this call as a constructor call so Return knows to return receiver if no object returned
                        m_frames.back().receiver = receiver;
                    } else {
                        runtime_error("Attempted to construct a non-function"_s);
                    }
                    VM_NEXT();
                }
                VM_CASE(This): {
                    push(current_this());
                    VM_NEXT();
                }
                VM_CASE(Return): {
                    while (!m_handlers.empty() && m_handlers.back().frame_index == m_frames.size() - 1) {
                        m_handlers.pop_back();
                    }
                    Value ret = pop();
                    // For constructor calls, if the function returns undefined, return the receiver (new object)
                    Value receiver = frame.receiver;
                    if (!ret.is_object() && receiver.is_object()) {
                        ret = receiver;
                    }
                    // Save current frame's stack_base before popping - this is where we should reset the stack to
                    usize return_stack_base = frame.stack_base;
                    m_frames.pop_back();
                    if (!m_this_stack.empty()) {
                        m_this_stack.pop_back();
                    }
                    if (m_frames.empty()) {
                        m_last_value = ret;
                        return InterpretResult::Ok;
                    }
                    m_stack.resize(return_stack_base);
                    push(ret);
                    VM_NEXT();
                }
                VM_CASE(EnterWith): {
                    Value obj = pop();
                    enter_with_env(obj);
                    VM_NEXT();
                }
                VM_CASE(ExitWith): {
                    exit_with_env();
                    VM_NEXT();
                }

#if USE_COMPUTED_GOTO
        vm_exit:;  // Exit point for computed goto dispatch
        #undef frame
        #undef VM_DISPATCH
        #undef VM_CASE
        #undef VM_NEXT
#else
            } // end switch
        } // end while
        #undef VM_CASE
        #undef VM_NEXT
#endif
    } catch (const RuntimeError& e) {
        m_error_message = String(e.what());
        return InterpretResult::RuntimeError;
    }

    return InterpretResult::Ok;
}

void VM::call_function(VMFunctionObject* func_obj, usize arg_count, const Value& receiver) {
    if (!func_obj) {
        runtime_error("Attempted to call a non-function"_s);
    }
    auto fn = func_obj->function;
    auto env = std::make_shared<Environment>(func_obj->closure);
    env->bind_function(fn);

    for (usize i = 0; i < fn->params.size(); ++i) {
        Value arg = (i < arg_count) ? peek(arg_count - 1 - i) : Value::undefined();
        env->set_local(static_cast<u16>(i), arg);
    }

    for (usize i = 0; i < arg_count + 1; ++i) {
        pop();
    }

    m_this_stack.push_back(receiver);
    m_frames.push_back(CallFrame{fn, env, env, 0, m_stack.size(), receiver, {}});
    m_frames.back().init_ic_cache();
}

void VM::handle_exception(const Value& thrown) {
    while (true) {
        if (m_handlers.empty()) {
            runtime_error("Unhandled exception"_s);
        }
        auto handler = m_handlers.back();
        m_handlers.pop_back();

        while (m_frames.size() > 0 && m_frames.size() - 1 > handler.frame_index) {
            m_frames.pop_back();
        }
        while (!m_handlers.empty() && m_handlers.back().frame_index >= m_frames.size()) {
            m_handlers.pop_back();
        }

        if (m_frames.empty()) {
            runtime_error("Unhandled exception"_s);
        }

        auto& frame = m_frames.back();
        while (m_this_stack.size() > m_frames.size()) {
            m_this_stack.pop_back();
        }
        m_stack.resize(frame.stack_base);
        if (handler.has_catch && handler.catch_ip != 0) {
            frame.ip = handler.catch_ip;
            push(thrown);
            return;
        }
        if (handler.finally_ip != 0) {
            frame.ip = handler.finally_ip;
            push(thrown);
            return;
        }
    }
}

void VM::enter_with_env(const Value& object) {
    if (!object.is_object()) {
        runtime_error("with target is not an object"_s);
    }
    auto& frame = m_frames.back();
    frame.env = std::make_shared<Environment>(frame.env, object);
}

void VM::exit_with_env() {
    auto& frame = m_frames.back();
    if (frame.env && frame.env->parent()) {
        frame.env = frame.env->parent();
    }
}

Value VM::current_this() const {
    if (m_this_stack.empty()) {
        return Value::undefined();
    }
    return m_this_stack.back();
}

void VM::runtime_error(const String& message) {
    m_diagnostics.add(DiagnosticStage::Runtime, DiagnosticLevel::Error, message);
    throw RuntimeError(message);
}

void VM::runtime_error(ErrorType error_type, const String& message, usize line, usize column) {
    // If no line/column provided, try to get from current call frame
    if (line == 0 && !m_frames.empty()) {
        const auto& frame = m_frames.back();
        if (frame.function) {
            // Get location from bytecode debug info
            BytecodeLocation loc = frame.function->chunk.get_location(frame.ip);
            if (loc.is_valid()) {
                line = loc.line;
                column = loc.column;
            }
        }
    }

    Diagnostic diag;
    diag.stage = DiagnosticStage::Runtime;
    diag.level = DiagnosticLevel::Error;
    diag.error_type = error_type;
    diag.message = message;
    diag.file = m_source_file;
    diag.line = line;
    diag.column = column;

    // Extract the source line if we have line information
    if (line > 0 && !m_source_code.empty()) {
        std::string_view src(m_source_code.c_str());
        usize current_line = 1;
        usize line_start = 0;

        for (usize i = 0; i < src.length(); ++i) {
            if (current_line == line) {
                // Found the target line, find its end
                usize line_end = i;
                while (line_end < src.length() && src[line_end] != '\n') {
                    line_end++;
                }
                diag.source_line = String(std::string(src.substr(i, line_end - i)));
                break;
            }
            if (src[i] == '\n') {
                current_line++;
            }
        }
    }

    // Build stack trace from call frames with accurate line numbers
    for (const auto& frame : m_frames) {
        if (frame.function) {
            StackFrame sf;
            sf.function_name = frame.function->name.empty() ? "<anonymous>"_s : frame.function->name;
            sf.file = m_source_file;

            // Get accurate location from bytecode debug info
            BytecodeLocation loc = frame.function->chunk.get_location(frame.ip);
            sf.line = loc.is_valid() ? loc.line : line;
            sf.column = loc.is_valid() ? loc.column : column;

            diag.stack_trace.push_back(std::move(sf));
        }
    }

    m_diagnostics.add(std::move(diag));
    m_error_message = message;
    throw RuntimeError(message);
}

// ============================================================================
// Garbage Collection Support
// ============================================================================

void VM::mark_roots(GarbageCollector& gc) {
    // Mark all values on the stack
    for (const auto& value : m_stack) {
        gc.mark_value(value);
    }

    // Mark all call frames
    for (const auto& frame : m_frames) {
        // Mark receiver
        gc.mark_value(frame.receiver);

        // Mark environment (which will mark locals and parent environments)
        if (frame.env) {
            // Mark all local variables
            for (const auto& [name, binding] : frame.env->m_values) {
                gc.mark_value(binding.value);
            }
            for (const auto& local : frame.env->m_locals) {
                gc.mark_value(local);
            }
            // Mark with-object if present
            if (frame.env->m_with_object) {
                gc.mark_value(frame.env->m_with_object.value());
            }
        }

        // Mark lexical environment
        if (frame.lexical_env && frame.lexical_env != frame.env) {
            for (const auto& [name, binding] : frame.lexical_env->m_values) {
                gc.mark_value(binding.value);
            }
            for (const auto& local : frame.lexical_env->m_locals) {
                gc.mark_value(local);
            }
        }
    }

    // Mark global environment
    if (m_global_env) {
        for (const auto& [name, binding] : m_global_env->m_values) {
            gc.mark_value(binding.value);
        }
        for (const auto& local : m_global_env->m_locals) {
            gc.mark_value(local);
        }
    }

    // Mark this stack
    for (const auto& value : m_this_stack) {
        gc.mark_value(value);
    }

    // Mark global objects
    if (m_global_object) {
        gc.mark_object(m_global_object.get());
    }
    if (m_object_prototype) {
        gc.mark_object(m_object_prototype.get());
    }
    if (m_function_prototype) {
        gc.mark_object(m_function_prototype.get());
    }
    if (m_array_prototype) {
        gc.mark_object(m_array_prototype.get());
    }

    // Mark last value
    gc.mark_value(m_last_value);
}

} // namespace lithium::js
