#pragma once

#include "lithium/core/string.hpp"
#include "lithium/core/types.hpp"
#include "lithium/js/value.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lithium::js {

// ============================================================================
// Bytecode Instructions (stack-based)
// ============================================================================

enum class OpCode : u8 {
    // Constants
    LoadConst,      // u16 idx
    LoadNull,
    LoadUndefined,
    LoadTrue,
    LoadFalse,

    // Stack
    Pop,
    Dup,
    Dup2,  // Duplicate top two stack values

    // Variables (by name idx into constant pool holding string)
    DefineVar,      // u16 name_idx, u8 is_const
    GetVar,         // u16 name_idx
    SetVar,         // u16 name_idx
    GetLocal,       // u16 slot_idx
    SetLocal,       // u16 slot_idx

    // Property access (slow path, no IC)
    GetProp,        // u16 name_idx
    SetProp,        // u16 name_idx
    GetElem,
    SetElem,

    // Property access with Inline Cache (fast path)
    GetPropIC,      // u16 name_idx, u16 cache_slot
    SetPropIC,      // u16 name_idx, u16 cache_slot

    // Arithmetic / comparison
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Exponent,
    LeftShift,
    RightShift,
    UnsignedRightShift,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    BitwiseNot,
    Negate,
    StrictEqual,
    StrictNotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,
    Instanceof,
    In,
    Typeof,
    Void,
    LogicalNot,

    // Control flow
    Jump,           // i16 offset
    JumpIfFalse,    // i16 offset (peek)
    JumpIfNullish,  // i16 offset (peek)
    Throw,          // throw value on top
    PushHandler,    // u16 catch_ip, u16 finally_ip, u8 has_catch
    PopHandler,     // no operand

    // Literals
    MakeArray,      // u16 count
    ArrayPush,      // push value into array (array, value -> array)
    ArraySpread,    // spread iterable (array, value -> array)
    MakeObject,     // no operand
    ObjectSpread,   // (object, source -> object)

    // Functions
    MakeFunction,   // u16 function_idx
    Call,           // u16 arg_count
    New,            // u16 arg_count (construct)
    NewStack,       // u16 arg_count (construct with stack allocation - escape analysis)
    Return,         // no operand (uses top of stack or undefined)

    // This binding
    This,           // load current this value

    // Dynamic scope helpers
    EnterWith,      // none (object on stack)
    ExitWith,       // none

    // Must be last - used for dispatch table sizing
    OpCode_COUNT
};

// ============================================================================
// Chunk: bytecode + constants
// ============================================================================

class Chunk {
public:
    void write(OpCode op);
    void write_u8(u8 byte);
    void write_u16(u16 value);
    void write_i16(i16 value);

    [[nodiscard]] usize size() const { return m_code.size(); }
    [[nodiscard]] u8 read(usize offset) const { return m_code[offset]; }
    [[nodiscard]] u16 read_u16(usize offset) const;
    [[nodiscard]] i16 read_i16(usize offset) const;

    [[nodiscard]] u16 add_constant(const Value& value);
    [[nodiscard]] const std::vector<Value>& constants() const { return m_constants; }

    // Patch helpers (offset points to operand bytes)
    void patch_i16(usize operand_offset, i16 value);

    [[nodiscard]] const std::vector<u8>& code() const { return m_code; }

private:
    std::vector<u8> m_code;
    std::vector<Value> m_constants;
};

// ============================================================================
// Function & Module
// ============================================================================

// Forward declaration for IC
struct InlineCacheEntry;

struct FunctionCode {
    String name;
    std::vector<String> params;
    Chunk chunk;

    // Inline cache slots for this function
    u16 ic_slot_count{0};

    // Local slots for parameters and variable declarations
    u16 local_count{0};
    std::vector<String> local_names;
    std::vector<bool> local_is_const;
    std::unordered_map<String, u16> local_slots;

    std::optional<u16> resolve_local(const String& name) const {
        auto it = local_slots.find(name);
        if (it == local_slots.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    u16 add_local(const String& name, bool is_const) {
        auto it = local_slots.find(name);
        if (it != local_slots.end()) {
            auto idx = it->second;
            if (is_const && idx < local_is_const.size()) {
                local_is_const[idx] = true;
            }
            return idx;
        }
        u16 slot = local_count++;
        local_slots.emplace(name, slot);
        local_names.push_back(name);
        local_is_const.push_back(is_const);
        return slot;
    }

    // Allocate a new IC slot and return its index
    u16 alloc_ic_slot() { return ic_slot_count++; }
};

struct ModuleBytecode {
    std::vector<std::shared_ptr<FunctionCode>> functions;
    u16 entry{0};
};

} // namespace lithium::js
