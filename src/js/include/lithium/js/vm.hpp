#pragma once

#include "lithium/js/bytecode.hpp"
#include "lithium/js/object.hpp"
#include "lithium/js/value.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace lithium::js {

class Compiler;
struct VMFunctionObject;

// ============================================================================
// VM - Bytecode interpreter
// ============================================================================

    class VM {
    public:
        VM();

    enum class InterpretResult {
        Ok,
        ParseError,
        RuntimeError
    };

    [[nodiscard]] InterpretResult interpret(const String& source);
    [[nodiscard]] const String& error_message() const { return m_error_message; }
    [[nodiscard]] const Value& last_value() const { return m_last_value; }

    void define_native(const String& name, NativeFn fn, u8 arity = 0);

private:
    struct Binding {
        Value value;
        bool is_const{false};
    };

public:
    class Environment {
    public:
        explicit Environment(std::shared_ptr<Environment> parent = nullptr);
        Environment(std::shared_ptr<Environment> parent, Value with_object);

        void define(const String& name, Value value, bool is_const);
        bool assign(const String& name, const Value& value);
        [[nodiscard]] std::optional<Binding> get(const String& name) const;

        [[nodiscard]] std::shared_ptr<Environment> parent() const { return m_parent; }
        [[nodiscard]] bool is_with_env() const { return m_with_object.has_value(); }

    private:
        std::shared_ptr<Environment> m_parent;
        std::unordered_map<String, Binding> m_values;
        std::optional<Value> m_with_object;
    };

    private:
    struct CallFrame {
        std::shared_ptr<FunctionCode> function;
        std::shared_ptr<Environment> env;
        usize ip{0};
        usize stack_base{0};
    };

    struct ExceptionHandler {
        usize frame_index{0};
        usize catch_ip{0};
        usize finally_ip{0};
        bool has_catch{false};
    };

    // Execution helpers
    void push(const Value& value);
    Value pop();
    Value& peek(usize distance = 0);
    const Value& peek(usize distance = 0) const;

    [[nodiscard]] Value read_constant(const CallFrame& frame, u16 idx) const;

    void call_function(VMFunctionObject* func_obj, usize arg_count);
    void handle_exception(const Value& thrown);
    void enter_with_env(const Value& object);
    void exit_with_env();

    // Error handling
    void runtime_error(const String& message);

    ModuleBytecode m_module;
    std::vector<CallFrame> m_frames;
    std::vector<Value> m_stack;
    std::vector<ExceptionHandler> m_handlers;

    std::shared_ptr<Environment> m_global_env;
    std::vector<std::shared_ptr<Environment>> m_env_stack;

    Value m_last_value;
    String m_error_message;
};

} // namespace lithium::js
