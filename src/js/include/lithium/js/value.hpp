#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
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
// JavaScript Value
// ============================================================================

// Value type tags
enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Object,
    Symbol,  // For future use
};

// Native value representation
class Value {
public:
    // Constructors
    Value();  // undefined
    Value(std::nullptr_t);  // null
    Value(bool b);
    Value(f64 n);
    Value(i32 n);
    Value(const char* s);
    Value(const String& s);
    Value(Object* obj);
    Value(std::shared_ptr<Object> obj);

    // Type checking
    [[nodiscard]] ValueType type() const { return m_type; }
    [[nodiscard]] bool is_undefined() const { return m_type == ValueType::Undefined; }
    [[nodiscard]] bool is_null() const { return m_type == ValueType::Null; }
    [[nodiscard]] bool is_nullish() const { return is_undefined() || is_null(); }
    [[nodiscard]] bool is_boolean() const { return m_type == ValueType::Boolean; }
    [[nodiscard]] bool is_number() const { return m_type == ValueType::Number; }
    [[nodiscard]] bool is_string() const { return m_type == ValueType::String; }
    [[nodiscard]] bool is_object() const { return m_type == ValueType::Object; }

    // Object type checking
    [[nodiscard]] bool is_function() const;
    [[nodiscard]] bool is_native_function() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_callable() const;

    // Value access (with type checks)
    [[nodiscard]] bool as_boolean() const;
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

private:
    ValueType m_type{ValueType::Undefined};
    union {
        bool boolean;
        f64 number;
    } m_primitive{};
    std::shared_ptr<Object> m_object;
    std::shared_ptr<String> m_string;
};

// ============================================================================
// NaN Boxing (optional optimization for 64-bit platforms)
// ============================================================================

// Alternative compact value representation using NaN-boxing
// All values fit in 64 bits by exploiting the NaN space of IEEE 754 doubles
//
// Layout:
// - If bits [51:0] are not all 1s: it's a double
// - If bits [51:0] are all 1s (signaling NaN):
//   - Bits [50:48] encode type tag
//   - Bits [47:0] encode payload (pointer or immediate value)
//
// This is not implemented here but noted for future optimization

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
