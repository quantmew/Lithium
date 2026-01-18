#pragma once

#include "lithium/js/bytecode.hpp"
#include "lithium/js/object.hpp"
#include "lithium/js/value.hpp"
#include "lithium/js/diagnostic.hpp"
#include "lithium/js/gc.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace lithium::js {

class Compiler;
struct VMFunctionObject;
class GarbageCollector;

// ============================================================================
// VM - Bytecode interpreter
// ============================================================================

    class VM {
        friend class GarbageCollector;  // GC needs access to mark_roots()

    public:
        VM();

    enum class InterpretResult {
        Ok,
        ParseError,
        RuntimeError
    };

    [[nodiscard]] InterpretResult interpret(const String& source, const String& filename = "<script>"_s);
    [[nodiscard]] const String& error_message() const { return m_error_message; }
    [[nodiscard]] const Value& last_value() const { return m_last_value; }
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return m_diagnostics.diagnostics(); }

    void define_native(const String& name, NativeFn fn, u8 arity = 0);
    void set_global(const String& name, const Value& value, bool is_const = true);

    // GC access
    [[nodiscard]] GarbageCollector& gc() { return m_gc; }

private:
    struct Binding {
        Value value;
        bool is_const{false};
    };

public:
    class Environment {
    public:
        explicit Environment(std::shared_ptr<Environment> parent = nullptr, std::shared_ptr<Object> global_object = nullptr);
        Environment(std::shared_ptr<Environment> parent, Value with_object);

        void define(const String& name, Value value, bool is_const);
        bool assign(const String& name, const Value& value);
        [[nodiscard]] std::optional<Binding> get(const String& name) const;
        void bind_function(const std::shared_ptr<FunctionCode>& function);
        [[nodiscard]] Value get_local(u16 slot) const;
        bool set_local(u16 slot, const Value& value);

        [[nodiscard]] std::shared_ptr<Environment> parent() const { return m_parent; }
        [[nodiscard]] bool is_with_env() const { return m_with_object.has_value(); }
        [[nodiscard]] std::shared_ptr<Object> global_object() const { return m_global_object; }

    private:
        std::shared_ptr<Environment> m_parent;
        std::unordered_map<String, Binding> m_values;
        std::vector<Value> m_locals;
        std::vector<bool> m_local_is_const;
        std::shared_ptr<FunctionCode> m_function;
        std::optional<Value> m_with_object;
        std::shared_ptr<Object> m_global_object;
        bool m_is_global{false};
        friend class VM;
    };

    private:
    struct CallFrame {
        std::shared_ptr<FunctionCode> function;
        std::shared_ptr<Environment> env;
        std::shared_ptr<Environment> lexical_env;
        usize ip{0};
        usize stack_base{0};
        Value receiver;

        // Inline cache for this function invocation
        std::vector<InlineCacheEntry> ic_cache;

        // Initialize IC cache with proper size
        void init_ic_cache() {
            if (function && function->ic_slot_count > 0) {
                ic_cache.resize(function->ic_slot_count);
            }
        }
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

    void call_function(VMFunctionObject* func_obj, usize arg_count, const Value& receiver);
    void handle_exception(const Value& thrown);
    void enter_with_env(const Value& object);
    void exit_with_env();
    [[nodiscard]] Value current_this() const;

    // Error handling
    void runtime_error(const String& message);
    void runtime_error(ErrorType error_type, const String& message, usize line = 0, usize column = 0);

    void init_builtins();
    void install_global(const String& name, const Value& value, bool is_const = true);

    // GC support - mark all roots for garbage collection
    void mark_roots(GarbageCollector& gc);

    // Garbage collector
    GarbageCollector m_gc;

    ModuleBytecode m_module;
    std::vector<CallFrame> m_frames;
    std::vector<Value> m_stack;
    std::vector<ExceptionHandler> m_handlers;

    std::shared_ptr<Environment> m_global_env;
    std::vector<std::shared_ptr<Environment>> m_env_stack;
    std::vector<Value> m_this_stack;
    std::shared_ptr<Object> m_global_object;
    std::shared_ptr<Object> m_object_prototype;
    std::shared_ptr<Object> m_function_prototype;
    std::shared_ptr<Object> m_array_prototype;
    DiagnosticSink m_diagnostics;

    Value m_last_value;
    String m_error_message;

    // Source code storage for error reporting
    String m_source_code;
    String m_source_file;
};

} // namespace lithium::js
