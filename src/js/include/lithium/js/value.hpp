#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <cstring>
#include <functional>
#include <memory>
#include <variant>
#include <vector>

namespace lithium::js {

// Forward declarations
class Value;
class Object;
class NativeFunction;
class BoundFunction;
class Array;
class VM;

using NativeFn = std::function<Value(VM& vm, const std::vector<Value>& args)>;

// ============================================================================
// NaN-Boxing: Compact 64-bit Value Representation
// ============================================================================
//
// IEEE 754 double-precision format:
// [Sign:1][Exponent:11][Mantissa:52]
//
// A quiet NaN has exponent=0x7FF and mantissa bit 51 set (quiet NaN bit).
// We use the remaining bits to encode type tags and payloads.
//
// Encoding scheme:
// - If (bits & QNAN_MASK) != QNAN_MASK: it's a regular double
// - Otherwise, it's a tagged value:
//   - Bits 50-48: Type tag (0-7)
//   - Bits 47-0:  Payload (48 bits, enough for x64 pointers)
//
// Tag values (in bits 50-48):
//   0 = Undefined
//   1 = Null
//   2 = Boolean (payload bit 0 = value)
//   3 = Integer (payload bits 31-0 = i32 value)
//   4 = String pointer
//   5 = Object pointer
//
// ============================================================================

// NaN-boxing constants
constexpr u64 SIGN_BIT       = 0x8000000000000000ULL;
constexpr u64 EXPONENT_MASK  = 0x7FF0000000000000ULL;
constexpr u64 MANTISSA_MASK  = 0x000FFFFFFFFFFFFFULL;
constexpr u64 QNAN_BIT       = 0x0008000000000000ULL;  // Quiet NaN bit (bit 51)
constexpr u64 QNAN_MASK      = EXPONENT_MASK | QNAN_BIT;  // 0x7FF8000000000000

// Tag bits (bits 50-48)
constexpr u64 TAG_SHIFT      = 48;
constexpr u64 TAG_MASK       = 0x0007000000000000ULL;  // 3 bits for tag
constexpr u64 PAYLOAD_MASK   = 0x0000FFFFFFFFFFFFULL;  // 48 bits for payload

// Tag values
constexpr u64 TAG_UNDEFINED  = 0ULL << TAG_SHIFT;
constexpr u64 TAG_NULL       = 1ULL << TAG_SHIFT;
constexpr u64 TAG_BOOLEAN    = 2ULL << TAG_SHIFT;
constexpr u64 TAG_INTEGER    = 3ULL << TAG_SHIFT;
constexpr u64 TAG_STRING     = 4ULL << TAG_SHIFT;
constexpr u64 TAG_OBJECT     = 5ULL << TAG_SHIFT;

// Full tag patterns (QNAN + tag)
constexpr u64 UNDEFINED_BITS = QNAN_MASK | TAG_UNDEFINED;
constexpr u64 NULL_BITS      = QNAN_MASK | TAG_NULL;
constexpr u64 FALSE_BITS     = QNAN_MASK | TAG_BOOLEAN | 0;
constexpr u64 TRUE_BITS      = QNAN_MASK | TAG_BOOLEAN | 1;

// Value type tags (for API compatibility)
enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Object,
    Symbol,  // For future use
};

// ============================================================================
// NaN-Boxed Value Class
// ============================================================================

class Value {
public:
    // Constructors
    Value() : m_bits(UNDEFINED_BITS) {}
    Value(std::nullptr_t) : m_bits(NULL_BITS) {}
    Value(bool b) : m_bits(b ? TRUE_BITS : FALSE_BITS) {}

    Value(f64 n) {
        std::memcpy(&m_bits, &n, sizeof(f64));
        // If the double happens to look like one of our tagged values, canonicalize it.
        // This is rare in practice - only happens with specific NaN bit patterns.
        if ((m_bits & QNAN_MASK) == QNAN_MASK && (m_bits & TAG_MASK) != 0) {
            // Use a safe canonical NaN (all zeros in tag bits)
            m_bits = QNAN_MASK;
        }
    }

    Value(i32 n) : m_bits(QNAN_MASK | TAG_INTEGER | (static_cast<u64>(static_cast<u32>(n)) & 0xFFFFFFFF)) {}

    Value(const char* s);
    Value(const String& s);
    Value(String* s);  // Takes ownership
    Value(Object* obj);
    Value(std::shared_ptr<Object> obj);

    // Fast inline check for pointer types (string or object)
    [[nodiscard]] bool is_heap_type() const {
        // Pointer types have tag >= 4 (bit 50 set in the tag area)
        return (m_bits & QNAN_MASK) == QNAN_MASK && (m_bits & (4ULL << TAG_SHIFT)) != 0;
    }

    // Copy/move semantics - inlined for primitives, call helper for heap types
    Value(const Value& other) : m_bits(other.m_bits) {
        if (is_heap_type()) inc_ref();
    }

    Value(Value&& other) noexcept : m_bits(other.m_bits) {
        other.m_bits = UNDEFINED_BITS;
    }

    Value& operator=(const Value& other) {
        if (this != &other) {
            if (is_heap_type()) dec_ref();
            m_bits = other.m_bits;
            if (is_heap_type()) inc_ref();
        }
        return *this;
    }

    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            if (is_heap_type()) dec_ref();
            m_bits = other.m_bits;
            other.m_bits = UNDEFINED_BITS;
        }
        return *this;
    }

    ~Value() {
        if (is_heap_type()) dec_ref();
    }

    // Type checking
    [[nodiscard]] ValueType type() const;

    [[nodiscard]] bool is_double() const {
        return (m_bits & QNAN_MASK) != QNAN_MASK;
    }

    [[nodiscard]] bool is_undefined() const { return m_bits == UNDEFINED_BITS; }
    [[nodiscard]] bool is_null() const { return m_bits == NULL_BITS; }
    [[nodiscard]] bool is_nullish() const { return is_undefined() || is_null(); }

    [[nodiscard]] bool is_boolean() const {
        return (m_bits & (QNAN_MASK | TAG_MASK)) == (QNAN_MASK | TAG_BOOLEAN);
    }

    [[nodiscard]] bool is_number() const {
        // Number if it's a double OR an integer
        return is_double() || is_integer();
    }

    [[nodiscard]] bool is_integer() const {
        return (m_bits & (QNAN_MASK | TAG_MASK)) == (QNAN_MASK | TAG_INTEGER);
    }

    [[nodiscard]] bool is_string() const {
        return (m_bits & (QNAN_MASK | TAG_MASK)) == (QNAN_MASK | TAG_STRING);
    }

    [[nodiscard]] bool is_object() const {
        return (m_bits & (QNAN_MASK | TAG_MASK)) == (QNAN_MASK | TAG_OBJECT);
    }

    // Object type checking
    [[nodiscard]] bool is_function() const;
    [[nodiscard]] bool is_native_function() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_callable() const;

    // Value access (with type checks)
    [[nodiscard]] bool as_boolean() const {
        if (!is_boolean()) return false;
        return (m_bits & 1) != 0;
    }

    [[nodiscard]] f64 as_number() const;
    [[nodiscard]] const String& as_string() const;
    [[nodiscard]] Object* as_object() const;
    [[nodiscard]] NativeFunction* as_native_function() const;

    template<typename T>
    [[nodiscard]] T* as() const {
        return dynamic_cast<T*>(as_object());
    }

    // Type conversion (ToBoolean, ToNumber, ToString as per spec)
    [[nodiscard]] bool to_boolean() const;
    [[nodiscard]] f64 to_number() const;
    [[nodiscard]] i32 to_int32() const;
    [[nodiscard]] u32 to_uint32() const;
    [[nodiscard]] String to_string() const;
    [[nodiscard]] bool is_truthy() const { return to_boolean(); }

    // Comparison
    [[nodiscard]] bool strict_equals(const Value& other) const;
    [[nodiscard]] bool loose_equals(const Value& other) const;
    [[nodiscard]] bool equals(const Value& other) const { return loose_equals(other); }

    // Operators for convenience
    [[nodiscard]] bool operator==(const Value& other) const { return strict_equals(other); }
    [[nodiscard]] bool operator!=(const Value& other) const { return !strict_equals(other); }

    // Type name (for typeof)
    [[nodiscard]] String typeof_string() const;

    // Debug representation
    [[nodiscard]] String debug_string() const;

    // Mark for GC
    void mark() const;

    // Static constructors
    static Value undefined() { return Value(); }
    static Value null() { return Value(nullptr); }
    static Value number(f64 n) { return Value(n); }
    static Value boolean(bool b) { return Value(b); }
    static Value string(const String& s) { return Value(s); }
    static Value object(std::shared_ptr<Object> obj) { return Value(std::move(obj)); }
    static Value native_function(NativeFn fn, u8 arity = 0, const String& name = ""_s);

    // Raw access for VM optimizations
    [[nodiscard]] u64 raw_bits() const { return m_bits; }

private:
    // Helper to extract pointer from payload
    template<typename T>
    [[nodiscard]] T* get_pointer() const {
        return reinterpret_cast<T*>(m_bits & PAYLOAD_MASK);
    }

    // Helper to create tagged pointer value
    static u64 make_tagged_ptr(u64 tag, void* ptr) {
        return QNAN_MASK | tag | (reinterpret_cast<u64>(ptr) & PAYLOAD_MASK);
    }

    // Increment ref count for pointer types
    void inc_ref() const;
    // Decrement ref count for pointer types
    void dec_ref() const;

    u64 m_bits;
};

// ============================================================================
// Value Operations
// ============================================================================

namespace value_ops {

// Arithmetic
[[nodiscard]] Value add(const Value& lhs, const Value& rhs);
[[nodiscard]] Value subtract(const Value& lhs, const Value& rhs);
[[nodiscard]] Value multiply(const Value& lhs, const Value& rhs);
[[nodiscard]] Value divide(const Value& lhs, const Value& rhs);
[[nodiscard]] Value modulo(const Value& lhs, const Value& rhs);
[[nodiscard]] Value exponent(const Value& lhs, const Value& rhs);
[[nodiscard]] Value negate(const Value& val);

// Bitwise
[[nodiscard]] Value bitwise_not(const Value& val);
[[nodiscard]] Value bitwise_and(const Value& lhs, const Value& rhs);
[[nodiscard]] Value bitwise_or(const Value& lhs, const Value& rhs);
[[nodiscard]] Value bitwise_xor(const Value& lhs, const Value& rhs);
[[nodiscard]] Value left_shift(const Value& lhs, const Value& rhs);
[[nodiscard]] Value right_shift(const Value& lhs, const Value& rhs);
[[nodiscard]] Value unsigned_right_shift(const Value& lhs, const Value& rhs);

// Comparison
[[nodiscard]] Value less_than(const Value& lhs, const Value& rhs);
[[nodiscard]] Value less_equal(const Value& lhs, const Value& rhs);
[[nodiscard]] Value greater_than(const Value& lhs, const Value& rhs);
[[nodiscard]] Value greater_equal(const Value& lhs, const Value& rhs);

// Logical
[[nodiscard]] Value logical_not(const Value& val);

// Type checks
[[nodiscard]] Value typeof_op(const Value& val);
[[nodiscard]] Value instanceof_op(const Value& obj, const Value& constructor);
[[nodiscard]] Value in_op(const Value& key, const Value& obj);

} // namespace value_ops

} // namespace lithium::js
