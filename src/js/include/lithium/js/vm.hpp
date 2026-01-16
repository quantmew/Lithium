#pragma once

#include "value.hpp"
#include "object.hpp"
#include "bytecode.hpp"
#include "gc.hpp"
#include <stack>
#include <unordered_map>

namespace lithium::js {

// ============================================================================
// Call Frame
// ============================================================================

struct CallFrame {
    Closure* closure{nullptr};
    const u8* ip{nullptr};  // Instruction pointer
    Value* slots{nullptr};  // Base of local variables on stack
};

// ============================================================================
// VM - Virtual Machine for executing bytecode
// ============================================================================

class VM {
public:
    VM();
    ~VM();

    // Execute bytecode
    enum class InterpretResult {
        Ok,
        CompileError,
        RuntimeError
    };

    [[nodiscard]] InterpretResult interpret(const String& source);
    [[nodiscard]] InterpretResult interpret(std::shared_ptr<Function> function);

    // Run the VM
    [[nodiscard]] InterpretResult run();

    // Value stack operations
    void push(const Value& value);
    [[nodiscard]] Value pop();
    [[nodiscard]] Value& peek(usize distance = 0);
    [[nodiscard]] const Value& peek(usize distance = 0) const;

    // Global variables
    void define_global(const String& name, const Value& value);
    [[nodiscard]] Value get_global(const String& name) const;
    void set_global(const String& name, const Value& value);
    [[nodiscard]] bool has_global(const String& name) const;

    // Native functions
    void define_native(const String& name, NativeFn fn, u8 arity = 0);

    // Object creation
    [[nodiscard]] std::shared_ptr<Object> create_object();
    [[nodiscard]] std::shared_ptr<Array> create_array();
    [[nodiscard]] std::shared_ptr<Array> create_array(usize size);

    // Error handling
    void runtime_error(const String& message);
    void throw_exception(const Value& value);
    [[nodiscard]] bool has_error() const { return m_had_error; }
    [[nodiscard]] const String& error_message() const { return m_error_message; }

    // GC
    GarbageCollector& gc() { return m_gc; }
    void collect_garbage();

    // Debug
    void set_trace_execution(bool enabled) { m_trace_execution = enabled; }

    // Callbacks
    using OutputCallback = std::function<void(const String&)>;
    void set_output_callback(OutputCallback callback) { m_output_callback = std::move(callback); }

    // Current this value
    [[nodiscard]] Value current_this() const;

private:
    // Instruction execution
    void execute_instruction();

    // Helper methods
    [[nodiscard]] u8 read_byte();
    [[nodiscard]] u16 read_u16();
    [[nodiscard]] i16 read_i16();
    [[nodiscard]] Value read_constant();
    [[nodiscard]] String read_string();

    // Call handling
    bool call(Closure* closure, u8 arg_count);
    bool call_value(const Value& callee, u8 arg_count);
    bool call_native(NativeFunction* native, u8 arg_count);
    bool invoke(const String& name, u8 arg_count);
    bool invoke_from_class(Class* klass, const String& name, u8 arg_count);

    // Upvalue handling
    std::shared_ptr<UpvalueSlot> capture_upvalue(Value* local);
    void close_upvalues(Value* last);

    // Class handling
    void define_method(const String& name);
    bool bind_method(Class* klass, const String& name);

    // Property access
    [[nodiscard]] Value get_property(Object* obj, const String& name);
    void set_property(Object* obj, const String& name, const Value& value);

    // Stack management
    void reset_stack();

    // Stack
    static constexpr usize STACK_MAX = 256 * 64;
    static constexpr usize FRAMES_MAX = 64;

    std::array<Value, STACK_MAX> m_stack;
    Value* m_stack_top{nullptr};

    std::array<CallFrame, FRAMES_MAX> m_frames;
    usize m_frame_count{0};

    // Globals
    std::unordered_map<String, Value> m_globals;

    // Open upvalues (linked list)
    std::shared_ptr<UpvalueSlot> m_open_upvalues;
    std::vector<std::shared_ptr<UpvalueSlot>> m_all_upvalues;

    // Init string (for constructors)
    String m_init_string{"constructor"};

    // GC
    GarbageCollector m_gc;

    // Error state
    bool m_had_error{false};
    String m_error_message;

    // Exception handling
    struct ExceptionHandler {
        usize frame_index;
        const u8* catch_ip;
        const u8* finally_ip;
    };
    std::vector<ExceptionHandler> m_exception_handlers;
    std::optional<Value> m_pending_exception;

    // Debug
    bool m_trace_execution{false};

    // Output
    OutputCallback m_output_callback;
};

// ============================================================================
// Built-in Objects and Functions
// ============================================================================

namespace builtins {

// Initialize built-in objects
void register_builtins(VM& vm);

// Object
void register_object_prototype(VM& vm);

// Array
void register_array_prototype(VM& vm);

// String
void register_string_prototype(VM& vm);

// Number
void register_number_prototype(VM& vm);

// Math
void register_math_object(VM& vm);

// Console
void register_console_object(VM& vm);

// JSON
void register_json_object(VM& vm);

} // namespace builtins

} // namespace lithium::js
