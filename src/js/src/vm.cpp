/**
 * JavaScript VM - bytecode interpreter with lexical environments
 */

#include "lithium/js/vm.hpp"
#include "lithium/js/compiler.hpp"
#include "lithium/js/parser.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

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

void VM::Environment::define(const String& name, Value value, bool is_const) {
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

void VM::init_builtins() {
    // Object constructor
    define_native("Object"_s, [this](VM& vm, const std::vector<Value>& args) -> Value {
        Value arg = args.empty() ? Value(std::make_shared<Object>()) : args[0];
        if (!arg.is_object()) {
            arg = Value(std::make_shared<Object>());
        }
        auto* obj = arg.as_object();
        if (obj && !obj->prototype()) {
            obj->set_prototype(vm.m_object_prototype);
        }
        return arg;
    }, 1);

    // Array constructor
    define_native("Array"_s, [this](VM&, const std::vector<Value>& args) -> Value {
        auto arr = std::make_shared<Array>();
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
        auto nf = std::make_shared<NativeFunction>(nm, fn, arity);
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
    auto math = std::make_shared<Object>();
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
    auto json = std::make_shared<Object>();
    json->set_prototype(m_object_prototype);
    json->set_property("stringify"_s, make_fn("stringify"_s, [](VM&, const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].is_undefined()) {
            return Value("undefined"_s);
        }
        return args[0].to_string();
    }, 1));
    install_global("JSON"_s, Value(json), true);

    // Basic globals
    install_global("globalThis"_s, Value(m_global_object), true);
}

// ============================================================================
// VM
// ============================================================================

VM::VM() {
    m_object_prototype = std::make_shared<Object>();
    m_function_prototype = std::make_shared<Object>();
    m_function_prototype->set_prototype(m_object_prototype);
    m_array_prototype = std::make_shared<Array>();
    m_array_prototype->set_prototype(m_object_prototype);
    m_global_object = std::make_shared<Object>();
    m_global_object->set_prototype(m_object_prototype);

    m_global_env = std::make_shared<Environment>(nullptr, m_global_object);
    m_global_env->m_is_global = true;
    init_builtins();
}

void VM::define_native(const String& name, NativeFn fn, u8 arity) {
    auto native_obj = std::make_shared<NativeFunction>(name, std::move(fn), arity);
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

VM::InterpretResult VM::interpret(const String& source) {
    m_diagnostics.clear();
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
    auto frame_env = m_global_env;
    m_frames.push_back(CallFrame{entry_fn, frame_env, 0, 0, Value::undefined()});

    try {
        while (!m_frames.empty()) {
            auto& frame = m_frames.back();
            if (frame.ip >= frame.function->chunk.size()) {
                throw RuntimeError("Instruction pointer out of range"_s);
            }

            OpCode op = static_cast<OpCode>(frame.function->chunk.read(frame.ip++));
            switch (op) {
                case OpCode::LoadConst: {
                    u16 idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    push(read_constant(frame, idx));
                    break;
                }
                case OpCode::LoadNull:
                    push(Value(nullptr));
                    break;
                case OpCode::LoadUndefined:
                    push(Value::undefined());
                    break;
                case OpCode::LoadTrue:
                    push(Value(true));
                    break;
                case OpCode::LoadFalse:
                    push(Value(false));
                    break;
                case OpCode::Pop:
                    pop();
                    break;
                case OpCode::Dup:
                    push(peek());
                    break;
                case OpCode::Dup2: {
                    // Duplicate top two values: [a, b] -> [a, b, a, b]
                    Value b = peek(0);
                    Value a = peek(1);
                    push(a);
                    push(b);
                    break;
                }
                case OpCode::DefineVar: {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    bool is_const = frame.function->chunk.read(frame.ip++) != 0;
                    auto name = read_constant(frame, name_idx).to_string();
                    frame.env->define(name, Value::undefined(), is_const);
                    break;
                }
                case OpCode::GetVar: {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    auto binding = frame.env->get(name);
                    if (!binding) {
                        runtime_error("Undefined variable: "_s + name);
                    }
                    push(binding->value);
                    break;
                }
                case OpCode::SetVar: {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value val = pop();
                    if (!frame.env->assign(name, val)) {
                        runtime_error("Assignment to undeclared or const variable: "_s + name);
                    }
                    push(val);
                    break;
                }
                case OpCode::GetProp: {
                    u16 name_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto name = read_constant(frame, name_idx).to_string();
                    Value obj = pop();
                    if (!obj.is_object()) {
                        runtime_error("Cannot get property of non-object"_s);
                    }
                    Value prop = obj.as_object()->get_property(name);
                    if (prop.is_callable()) {
                        prop = Value(std::make_shared<BoundFunction>(prop, obj));
                    }
                    push(prop);
                    break;
                }
                case OpCode::SetProp: {
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
                    break;
                }
                case OpCode::GetElem: {
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
                        prop = Value(std::make_shared<BoundFunction>(prop, obj));
                    }
                    push(prop);
                    break;
                }
                case OpCode::SetElem: {
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
                    break;
                }
                case OpCode::Add: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::add(a, b));
                    break;
                }
                case OpCode::Subtract: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::subtract(a, b));
                    break;
                }
                case OpCode::Multiply: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::multiply(a, b));
                    break;
                }
                case OpCode::Divide: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::divide(a, b));
                    break;
                }
                case OpCode::Modulo: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::modulo(a, b));
                    break;
                }
                case OpCode::Exponent: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::exponent(a, b));
                    break;
                }
                case OpCode::LeftShift: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::left_shift(a, b));
                    break;
                }
                case OpCode::RightShift: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::right_shift(a, b));
                    break;
                }
                case OpCode::UnsignedRightShift: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::unsigned_right_shift(a, b));
                    break;
                }
                case OpCode::BitwiseAnd: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_and(a, b));
                    break;
                }
                case OpCode::BitwiseOr: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_or(a, b));
                    break;
                }
                case OpCode::BitwiseXor: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::bitwise_xor(a, b));
                    break;
                }
                case OpCode::BitwiseNot: {
                    Value v = pop();
                    push(value_ops::bitwise_not(v));
                    break;
                }
                case OpCode::Negate: {
                    Value v = pop();
                    push(value_ops::negate(v));
                    break;
                }
                case OpCode::StrictEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.strict_equals(b)));
                    break;
                }
                case OpCode::StrictNotEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(!a.strict_equals(b)));
                    break;
                }
                case OpCode::LessThan: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::less_than(a, b));
                    break;
                }
                case OpCode::LessEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::less_equal(a, b));
                    break;
                }
                case OpCode::GreaterThan: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::greater_than(a, b));
                    break;
                }
                case OpCode::GreaterEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::greater_equal(a, b));
                    break;
                }
                case OpCode::Instanceof: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::instanceof_op(a, b));
                    break;
                }
                case OpCode::In: {
                    Value b = pop();
                    Value a = pop();
                    push(value_ops::in_op(a, b));
                    break;
                }
                case OpCode::LogicalNot: {
                    Value v = pop();
                    push(Value(!v.to_boolean()));
                    break;
                }
                case OpCode::Typeof: {
                    Value v = pop();
                    push(value_ops::typeof_op(v));
                    break;
                }
                case OpCode::Void: {
                    push(Value::undefined());
                    break;
                }
                case OpCode::Jump: {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + 2 + offset);
                    break;
                }
                case OpCode::JumpIfFalse: {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip += 2;
                    if (!peek().to_boolean()) {
                        frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + offset);
                    }
                    break;
                }
                case OpCode::JumpIfNullish: {
                    i16 offset = frame.function->chunk.read_i16(frame.ip);
                    frame.ip += 2;
                    if (peek().is_nullish()) {
                        frame.ip = static_cast<usize>(static_cast<i32>(frame.ip) + offset);
                    }
                    break;
                }
                case OpCode::Throw: {
                    Value v = pop();
                    handle_exception(v);
                    break;
                }
                case OpCode::PushHandler: {
                    u16 catch_ip = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    u16 finally_ip = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    bool has_catch = frame.function->chunk.read(frame.ip++) != 0;
                    m_handlers.push_back(ExceptionHandler{m_frames.size() - 1, catch_ip, finally_ip, has_catch});
                    break;
                }
                case OpCode::PopHandler: {
                    if (!m_handlers.empty()) {
                        m_handlers.pop_back();
                    }
                    break;
                }
                case OpCode::MakeArray: {
                    u16 count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    auto arr = std::make_shared<Array>();
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
                    break;
                }
                case OpCode::ArrayPush: {
                    Value value = pop();
                    Value arr = pop();
                    if (!arr.is_array()) {
                        runtime_error("Cannot push into non-array"_s);
                    }
                    arr.as<Array>()->push(value);
                    push(arr);
                    break;
                }
                case OpCode::ArraySpread: {
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
                    break;
                }
                case OpCode::MakeObject: {
                    auto obj = std::make_shared<Object>();
                    obj->set_prototype(m_object_prototype);
                    push(Value(obj));
                    break;
                }
                case OpCode::ObjectSpread: {
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
                    break;
                }
                case OpCode::MakeFunction: {
                    u16 func_idx = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    if (func_idx >= m_module.functions.size()) {
                        runtime_error("Invalid function index"_s);
                    }
                    auto fn_obj = std::make_shared<VMFunctionObject>(m_module.functions[func_idx], frame.env);
                    fn_obj->set_prototype(m_function_prototype);
                    auto proto_obj = std::make_shared<Object>();
                    proto_obj->set_prototype(m_object_prototype);
                    proto_obj->set_property("constructor"_s, Value(fn_obj));
                    fn_obj->set_property("prototype"_s, Value(proto_obj));
                    fn_obj->set_property("length"_s, Value(static_cast<f64>(fn_obj->function->params.size())));
                    fn_obj->set_property("name"_s, Value(fn_obj->function->name));
                    push(Value(std::static_pointer_cast<Object>(fn_obj)));
                    break;
                }
                case OpCode::Call: {
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
                        runtime_error("Attempted to call a non-function"_s);
                    }
                    break;
                }
                case OpCode::New: {
                    u16 arg_count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    Value callee = peek(arg_count);
                    if (!callee.is_callable()) {
                        runtime_error("Attempted to construct a non-function"_s);
                    }
                    // Create a new object with the constructor's prototype
                    auto new_obj = std::make_shared<Object>();
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
                    break;
                }
                case OpCode::This: {
                    push(current_this());
                    break;
                }
                case OpCode::Return: {
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
                    break;
                }
                case OpCode::EnterWith: {
                    Value obj = pop();
                    enter_with_env(obj);
                    break;
                }
                case OpCode::ExitWith: {
                    exit_with_env();
                    break;
                }
            }
        }
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

    for (usize i = 0; i < fn->params.size(); ++i) {
        Value arg = (i < arg_count) ? peek(arg_count - 1 - i) : Value::undefined();
        env->define(fn->params[i], arg, false);
    }

    for (usize i = 0; i < arg_count + 1; ++i) {
        pop();
    }

    m_this_stack.push_back(receiver);
    m_frames.push_back(CallFrame{fn, env, 0, m_stack.size(), receiver});
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

} // namespace lithium::js
