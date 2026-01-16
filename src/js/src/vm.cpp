/**
 * JavaScript Virtual Machine implementation (stub)
 */

#include "lithium/js/vm.hpp"
#include "lithium/js/parser.hpp"
#include "lithium/js/compiler.hpp"
#include "lithium/js/gc.hpp"

namespace lithium::js {

// ============================================================================
// VM implementation
// ============================================================================

VM::VM() : m_gc(std::make_unique<GC>()) {
    // Initialize global object
    m_global = Value::object(m_gc->allocate<Object>());
}

VM::~VM() = default;

VM::InterpretResult VM::interpret(const String& source) {
    Parser parser;
    parser.set_input(source);

    auto program = parser.parse_program();
    if (!parser.errors().empty()) {
        m_error_message = parser.errors()[0];
        return InterpretResult::CompileError;
    }

    Compiler compiler;
    auto function = compiler.compile(*program);
    if (!function) {
        m_error_message = "Compilation failed"_s;
        return InterpretResult::CompileError;
    }

    return run(*function);
}

VM::InterpretResult VM::run(const Function& function) {
    // Initialize call frame
    m_frames.clear();
    m_frames.push_back(CallFrame{&function, 0, 0});
    m_stack.clear();

    while (!m_frames.empty()) {
        auto& frame = m_frames.back();
        const auto& chunk = frame.function->chunk;

        if (frame.ip >= chunk.code.size()) {
            m_frames.pop_back();
            continue;
        }

        auto instruction = static_cast<OpCode>(chunk.code[frame.ip++]);

        switch (instruction) {
            case OpCode::Constant: {
                u8 index = chunk.code[frame.ip++];
                push(chunk.constants[index]);
                break;
            }

            case OpCode::Null:
                push(Value::null());
                break;

            case OpCode::True:
                push(Value::boolean(true));
                break;

            case OpCode::False:
                push(Value::boolean(false));
                break;

            case OpCode::Pop:
                pop();
                break;

            case OpCode::GetLocal: {
                u8 slot = chunk.code[frame.ip++];
                push(m_stack[frame.stack_base + slot]);
                break;
            }

            case OpCode::SetLocal: {
                u8 slot = chunk.code[frame.ip++];
                m_stack[frame.stack_base + slot] = peek(0);
                break;
            }

            case OpCode::GetGlobal: {
                u8 index = chunk.code[frame.ip++];
                auto& name = chunk.constants[index];
                if (name.is_string()) {
                    auto it = m_globals.find(name.as_string());
                    if (it != m_globals.end()) {
                        push(it->second);
                    } else {
                        push(Value::undefined());
                    }
                }
                break;
            }

            case OpCode::SetGlobal: {
                u8 index = chunk.code[frame.ip++];
                auto& name = chunk.constants[index];
                if (name.is_string()) {
                    m_globals[name.as_string()] = peek(0);
                }
                break;
            }

            case OpCode::Add: {
                auto b = pop();
                auto a = pop();
                if (a.is_number() && b.is_number()) {
                    push(Value::number(a.as_number() + b.as_number()));
                } else {
                    push(Value::string(a.build() + b.build()));
                }
                break;
            }

            case OpCode::Subtract: {
                auto b = pop();
                auto a = pop();
                push(Value::number(a.as_number() - b.as_number()));
                break;
            }

            case OpCode::Multiply: {
                auto b = pop();
                auto a = pop();
                push(Value::number(a.as_number() * b.as_number()));
                break;
            }

            case OpCode::Divide: {
                auto b = pop();
                auto a = pop();
                push(Value::number(a.as_number() / b.as_number()));
                break;
            }

            case OpCode::Modulo: {
                auto b = pop();
                auto a = pop();
                push(Value::number(std::fmod(a.as_number(), b.as_number())));
                break;
            }

            case OpCode::Negate:
                push(Value::number(-pop().as_number()));
                break;

            case OpCode::Not:
                push(Value::boolean(!pop().is_truthy()));
                break;

            case OpCode::Equal: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.equals(b)));
                break;
            }

            case OpCode::NotEqual: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(!a.equals(b)));
                break;
            }

            case OpCode::StrictEqual: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.strict_equals(b)));
                break;
            }

            case OpCode::StrictNotEqual: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(!a.strict_equals(b)));
                break;
            }

            case OpCode::Less: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.as_number() < b.as_number()));
                break;
            }

            case OpCode::LessEqual: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.as_number() <= b.as_number()));
                break;
            }

            case OpCode::Greater: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.as_number() > b.as_number()));
                break;
            }

            case OpCode::GreaterEqual: {
                auto b = pop();
                auto a = pop();
                push(Value::boolean(a.as_number() >= b.as_number()));
                break;
            }

            case OpCode::Jump: {
                u16 offset = static_cast<u16>(chunk.code[frame.ip] << 8) |
                             chunk.code[frame.ip + 1];
                frame.ip += 2 + offset;
                break;
            }

            case OpCode::JumpIfFalse: {
                u16 offset = static_cast<u16>(chunk.code[frame.ip] << 8) |
                             chunk.code[frame.ip + 1];
                frame.ip += 2;
                if (!peek(0).is_truthy()) {
                    frame.ip += offset;
                }
                break;
            }

            case OpCode::Loop: {
                u16 offset = static_cast<u16>(chunk.code[frame.ip] << 8) |
                             chunk.code[frame.ip + 1];
                frame.ip += 2;
                frame.ip -= offset;
                break;
            }

            case OpCode::Call: {
                u8 arg_count = chunk.code[frame.ip++];
                auto callee = peek(arg_count);

                if (callee.is_function()) {
                    auto* func = callee.as_function();
                    CallFrame new_frame{func, 0, m_stack.size() - arg_count - 1};
                    m_frames.push_back(new_frame);
                } else if (callee.is_native_function()) {
                    auto native = callee.as_native_function();
                    std::vector<Value> args;
                    for (u8 i = 0; i < arg_count; ++i) {
                        args.insert(args.begin(), pop());
                    }
                    pop(); // Pop callee
                    push(native(*this, args));
                } else {
                    m_error_message = "Can only call functions"_s;
                    return InterpretResult::RuntimeError;
                }
                break;
            }

            case OpCode::Return: {
                Value result = pop();
                m_frames.pop_back();
                if (!m_frames.empty()) {
                    // Remove arguments and function from stack
                    push(result);
                }
                break;
            }

            case OpCode::GetProperty: {
                u8 index = chunk.code[frame.ip++];
                auto& name = chunk.constants[index];
                auto object = pop();
                if (object.is_object() && name.is_string()) {
                    push(object.as_object()->get(name.as_string()));
                } else {
                    push(Value::undefined());
                }
                break;
            }

            case OpCode::SetProperty: {
                u8 index = chunk.code[frame.ip++];
                auto& name = chunk.constants[index];
                auto value = pop();
                auto object = pop();
                if (object.is_object() && name.is_string()) {
                    object.as_object()->set(name.as_string(), value);
                }
                push(value);
                break;
            }

            case OpCode::CreateObject: {
                auto obj = m_gc->allocate<Object>();
                push(Value::object(obj));
                break;
            }

            case OpCode::CreateArray: {
                u8 count = chunk.code[frame.ip++];
                auto arr = m_gc->allocate<Array>();
                for (u8 i = 0; i < count; ++i) {
                    arr->elements.insert(arr->elements.begin(), pop());
                }
                push(Value::object(arr));
                break;
            }

            default:
                m_error_message = "Unknown opcode"_s;
                return InterpretResult::RuntimeError;
        }
    }

    return InterpretResult::Ok;
}

void VM::define_global(const String& name, const Value& value) {
    m_globals[name] = value;
}

void VM::define_native(const String& name, NativeFunction func) {
    m_globals[name] = Value::native_function(std::move(func));
}

void VM::push(const Value& value) {
    if (m_stack.size() >= STACK_MAX) {
        m_error_message = "Stack overflow"_s;
        return;
    }
    m_stack.push_back(value);
}

Value VM::pop() {
    if (m_stack.empty()) {
        return Value::undefined();
    }
    Value value = m_stack.back();
    m_stack.pop_back();
    return value;
}

Value VM::peek(usize distance) const {
    if (distance >= m_stack.size()) {
        return Value::undefined();
    }
    return m_stack[m_stack.size() - 1 - distance];
}

void VM::collect_garbage() {
    m_gc->collect();
}

} // namespace lithium::js
