#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <vector>

namespace lithium::js {

// ============================================================================
// Bytecode Instructions
// ============================================================================

enum class OpCode : u8 {
    // Stack manipulation
    Pop,            // Discard top of stack
    Dup,            // Duplicate top of stack
    Swap,           // Swap top two values

    // Constants
    LoadConst,      // Load constant from pool: idx
    LoadNull,       // Push null
    LoadUndefined,  // Push undefined
    LoadTrue,       // Push true
    LoadFalse,      // Push false
    LoadZero,       // Push 0
    LoadOne,        // Push 1

    // Variables
    GetLocal,       // Get local variable: slot
    SetLocal,       // Set local variable: slot
    GetGlobal,      // Get global variable: name_idx
    SetGlobal,      // Set global variable: name_idx
    GetUpvalue,     // Get captured variable: idx
    SetUpvalue,     // Set captured variable: idx
    DefineGlobal,   // Define global variable: name_idx

    // Object/Property access
    GetProperty,    // Get property: name_idx
    SetProperty,    // Set property: name_idx
    GetElement,     // Get element (computed)
    SetElement,     // Set element (computed)
    DeleteProperty, // Delete property

    // Arithmetic
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Exponent,
    Negate,         // Unary minus
    Increment,
    Decrement,

    // Bitwise
    BitwiseNot,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    LeftShift,
    RightShift,
    UnsignedRightShift,

    // Comparison
    Equal,
    NotEqual,
    StrictEqual,
    StrictNotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,

    // Logical
    Not,

    // Type operations
    Typeof,
    Instanceof,
    In,

    // Control flow
    Jump,           // Unconditional jump: offset (i16)
    JumpIfFalse,    // Jump if false (pop): offset (i16)
    JumpIfTrue,     // Jump if true (pop): offset (i16)
    JumpIfFalseKeep,// Jump if false (keep): offset (i16)
    JumpIfTrueKeep, // Jump if true (keep): offset (i16)
    Loop,           // Jump backward: offset (u16)

    // Functions
    Call,           // Call function: argc
    TailCall,       // Tail call: argc
    Return,         // Return from function
    Closure,        // Create closure: func_idx, upvalue_count

    // Objects
    CreateObject,   // Create empty object
    CreateArray,    // Create array: element_count
    DefineProperty, // Define property with flags

    // Class
    CreateClass,    // Create class: name_idx
    DefineMethod,   // Define method: name_idx
    GetSuper,       // Get super reference

    // Iteration
    GetIterator,    // Get iterator
    IteratorNext,   // Get next value
    IteratorClose,  // Close iterator

    // Exception handling
    Throw,          // Throw exception
    PushExceptionHandler, // Push handler: catch_offset, finally_offset
    PopExceptionHandler,  // Pop handler
    EnterFinally,   // Enter finally block
    LeaveFinally,   // Leave finally block

    // Misc
    This,           // Push this value
    SuperCall,      // Call super constructor: argc
    Spread,         // Spread array/object
    Debugger,       // Debugger statement
};

// ============================================================================
// Bytecode Chunk
// ============================================================================

struct LineInfo {
    usize offset;
    usize line;
};

class Chunk {
public:
    Chunk() = default;

    // Writing bytecode
    void write(OpCode op);
    void write(u8 byte);
    void write_u16(u16 value);
    void write_i16(i16 value);

    // Patch jump targets
    void patch_jump(usize offset);
    void patch_jump(usize offset, i16 target);

    // Add constant and return index
    [[nodiscard]] u16 add_constant(f64 value);
    [[nodiscard]] u16 add_constant(const String& value);
    [[nodiscard]] u16 add_constant(bool value);

    // Access
    [[nodiscard]] const std::vector<u8>& code() const { return m_code; }
    [[nodiscard]] u8 read(usize offset) const { return m_code[offset]; }
    [[nodiscard]] u16 read_u16(usize offset) const;
    [[nodiscard]] i16 read_i16(usize offset) const;

    [[nodiscard]] usize size() const { return m_code.size(); }

    // Line information
    void set_line(usize line);
    [[nodiscard]] usize get_line(usize offset) const;

    // Constants
    [[nodiscard]] const std::vector<std::variant<f64, String, bool>>& constants() const {
        return m_constants;
    }

    // Disassembly (for debugging)
    [[nodiscard]] String disassemble() const;
    [[nodiscard]] String disassemble_instruction(usize offset) const;

private:
    std::vector<u8> m_code;
    std::vector<std::variant<f64, String, bool>> m_constants;
    std::vector<LineInfo> m_lines;
    usize m_current_line{1};
};

// ============================================================================
// Function Object (compiled)
// ============================================================================

struct Upvalue {
    u8 index;
    bool is_local;
};

class Function {
public:
    Function(String name, u8 arity);

    [[nodiscard]] const String& name() const { return m_name; }
    [[nodiscard]] u8 arity() const { return m_arity; }
    [[nodiscard]] Chunk& chunk() { return m_chunk; }
    [[nodiscard]] const Chunk& chunk() const { return m_chunk; }

    // Upvalues
    void add_upvalue(u8 index, bool is_local);
    [[nodiscard]] const std::vector<Upvalue>& upvalues() const { return m_upvalues; }
    [[nodiscard]] usize upvalue_count() const { return m_upvalues.size(); }

    // Local count (for stack frame allocation)
    void set_local_count(u8 count) { m_local_count = count; }
    [[nodiscard]] u8 local_count() const { return m_local_count; }

private:
    String m_name;
    u8 m_arity{0};
    u8 m_local_count{0};
    Chunk m_chunk;
    std::vector<Upvalue> m_upvalues;
};

} // namespace lithium::js
