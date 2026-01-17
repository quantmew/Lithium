/**
 * JavaScript VM - bytecode interpreter with lexical environments
 */

#include "lithium/js/vm.hpp"
#include "lithium/js/compiler.hpp"
#include "lithium/js/parser.hpp"
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

VM::Environment::Environment(std::shared_ptr<Environment> parent)
    : m_parent(std::move(parent)) {}

VM::Environment::Environment(std::shared_ptr<Environment> parent, Value with_object)
    : m_parent(std::move(parent))
    , m_with_object(std::move(with_object)) {}

void VM::Environment::define(const String& name, Value value, bool is_const) {
    m_values[name] = Binding{std::move(value), is_const};
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
        return true;
    }
    if (m_parent) {
        return m_parent->assign(name, value);
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
// VM
// ============================================================================

VM::VM() {
    m_global_env = std::make_shared<Environment>();
}

void VM::define_native(const String& name, NativeFn fn, u8 arity) {
    m_global_env->define(name, Value::native_function(std::move(fn), arity, name), true);
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
    Parser parser;
    auto program = parser.parse(source);
    if (parser.has_errors()) {
        m_error_message = parser.errors().front();
        return InterpretResult::ParseError;
    }

    Compiler compiler;
    auto result = compiler.compile(*program);
    if (!result.errors.empty()) {
        m_error_message = result.errors.front();
        return InterpretResult::ParseError;
    }

    m_module = std::move(result.module);
    m_frames.clear();
    m_stack.clear();
    m_handlers.clear();

    auto entry_fn = m_module.functions[m_module.entry];
    auto frame_env = m_global_env;
    m_frames.push_back(CallFrame{entry_fn, frame_env, 0, 0});

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
                    push(obj.as_object()->get_property(name));
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
                    if (obj.is_array()) {
                        push(obj.as<Array>()->get_element(static_cast<u32>(key.to_uint32())));
                    } else {
                        push(obj.as_object()->get_property(key.to_string()));
                    }
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
                    push(Value(std::make_shared<Object>()));
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
                    push(Value(std::static_pointer_cast<Object>(fn_obj)));
                    break;
                }
                case OpCode::Call: {
                    u16 arg_count = frame.function->chunk.read_u16(frame.ip);
                    frame.ip += 2;
                    Value callee = peek(arg_count);
                    if (auto* native = callee.as_native_function()) {
                        std::vector<Value> args;
                        args.reserve(arg_count);
                        for (usize i = 0; i < arg_count; ++i) {
                            args.push_back(peek(arg_count - 1 - i));
                        }
                        for (usize i = 0; i < arg_count + 1; ++i) {
                            pop();
                        }
                        Value res = native->call(*this, args);
                        push(res);
                    } else if (auto* func_obj = dynamic_cast<VMFunctionObject*>(callee.as_object())) {
                        call_function(func_obj, arg_count);
                    } else {
                        runtime_error("Attempted to call a non-function"_s);
                    }
                    break;
                }
                case OpCode::Return: {
                    while (!m_handlers.empty() && m_handlers.back().frame_index == m_frames.size() - 1) {
                        m_handlers.pop_back();
                    }
                    Value ret = pop();
                    m_frames.pop_back();
                    if (m_frames.empty()) {
                        m_last_value = ret;
                        return InterpretResult::Ok;
                    }
                    m_stack.resize(m_frames.back().stack_base);
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

void VM::call_function(VMFunctionObject* func_obj, usize arg_count) {
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

    m_frames.push_back(CallFrame{fn, env, 0, m_stack.size()});
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

void VM::runtime_error(const String& message) {
    throw RuntimeError(message);
}

} // namespace lithium::js
