/**
 * JavaScript Value implementation with NaN-Boxing and String Interning
 */

#include "lithium/js/value.hpp"
#include "lithium/js/object.hpp"
#include "lithium/js/string_intern.hpp"
#include <atomic>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

namespace lithium::js {

// ============================================================================
// Reference counting for NaN-boxed pointers
// ============================================================================

// ============================================================================
// GC-managed objects - no reference counting
// ============================================================================
//
// Strings use StringInternPool with ref counting (they're small and shared)
// Objects are managed by GC - Value stores raw pointers, GC ensures liveness

void Value::inc_ref() const {
    // Only strings need ref counting (interned strings)
    if ((m_bits & TAG_MASK) == TAG_STRING) {
        auto* ref = reinterpret_cast<std::atomic<u32>*>(m_bits & PAYLOAD_MASK);
        ref->fetch_add(1, std::memory_order_relaxed);
    }
    // Objects: no-op, GC manages them
}

void Value::dec_ref() const {
    // Only strings need ref counting (interned strings)
    if ((m_bits & TAG_MASK) == TAG_STRING) {
        void* ptr = reinterpret_cast<void*>(m_bits & PAYLOAD_MASK);
        StringInternPool::instance().dec_ref(reinterpret_cast<InternedString*>(ptr));
    }
    // Objects: no-op, GC manages them
}

// ============================================================================
// Value constructors
// ============================================================================

Value::Value(const char* s) {
    // Use string interning for all strings
    auto* interned = StringInternPool::instance().intern(String(s));
    m_bits = make_tagged_ptr(TAG_STRING, interned);
}

Value::Value(const String& s) {
    // Use string interning for all strings
    auto* interned = StringInternPool::instance().intern(s);
    m_bits = make_tagged_ptr(TAG_STRING, interned);
}

Value::Value(String* s) {
    // Use string interning for all strings
    auto* interned = StringInternPool::instance().intern(*s);
    delete s;
    m_bits = make_tagged_ptr(TAG_STRING, interned);
}

Value::Value(Object* obj) {
    if (obj) {
        // Store raw pointer directly - GC manages lifetime
        m_bits = make_tagged_ptr(TAG_OBJECT, obj);
    } else {
        m_bits = NULL_BITS;
    }
}

Value::Value(std::shared_ptr<Object> obj) {
    if (obj) {
        // Store raw pointer directly - GC manages lifetime
        m_bits = make_tagged_ptr(TAG_OBJECT, obj.get());
    } else {
        m_bits = NULL_BITS;
    }
}

// Copy/Move semantics are now inline in the header for better optimization

// ============================================================================
// Type checking
// ============================================================================

ValueType Value::type() const {
    if (is_double()) return ValueType::Number;

    u64 tag = (m_bits & TAG_MASK) >> TAG_SHIFT;
    switch (tag) {
        case 0: return ValueType::Undefined;
        case 1: return ValueType::Null;
        case 2: return ValueType::Boolean;
        case 3: return ValueType::Number;  // Integer
        case 4: return ValueType::String;
        case 5: return ValueType::Object;
        default: return ValueType::Undefined;
    }
}

bool Value::is_function() const {
    return is_callable();
}

bool Value::is_callable() const {
    if (!is_object()) return false;
    auto* obj = as_object();
    return obj && obj->is_callable();
}

bool Value::is_native_function() const {
    return dynamic_cast<NativeFunction*>(as_object()) != nullptr;
}

bool Value::is_array() const {
    return dynamic_cast<Array*>(as_object()) != nullptr;
}

// ============================================================================
// Value access
// ============================================================================

f64 Value::as_number() const {
    if (is_double()) {
        f64 result;
        std::memcpy(&result, &m_bits, sizeof(f64));
        return result;
    }
    if (is_integer()) {
        // Extract i32 from lower 32 bits
        i32 val = static_cast<i32>(m_bits & 0xFFFFFFFF);
        return static_cast<f64>(val);
    }
    return std::numeric_limits<f64>::quiet_NaN();
}

const String& Value::as_string() const {
    static String empty;
    if (!is_string()) return empty;
    auto* interned = get_pointer<InternedString>();
    if (!interned) return empty;
    return interned->str;
}

Object* Value::as_object() const {
    if (!is_object()) return nullptr;
    // Return raw pointer directly (GC-managed)
    return get_pointer<Object>();
}

NativeFunction* Value::as_native_function() const {
    return dynamic_cast<NativeFunction*>(as_object());
}

// ============================================================================
// Type conversion
// ============================================================================

bool Value::to_boolean() const {
    if (is_undefined() || is_null()) return false;
    if (is_boolean()) return as_boolean();
    if (is_number()) {
        f64 n = as_number();
        return n != 0 && !std::isnan(n);
    }
    if (is_string()) {
        return !as_string().empty();
    }
    if (is_object()) {
        return as_object() != nullptr;
    }
    return false;
}

f64 Value::to_number() const {
    if (is_undefined()) return std::numeric_limits<f64>::quiet_NaN();
    if (is_null()) return 0.0;
    if (is_boolean()) return as_boolean() ? 1.0 : 0.0;
    if (is_number()) return as_number();
    if (is_string()) {
        const auto& str = as_string();
        if (str.empty()) return 0.0;

        bool seen_digit = false;
        bool seen_dot = false;
        for (usize i = 0; i < str.length(); ++i) {
            char ch = str[i];
            if (i == 0 && (ch == '+' || ch == '-')) {
                continue;
            }
            if (ch == '.' && !seen_dot) {
                seen_dot = true;
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return std::numeric_limits<f64>::quiet_NaN();
            }
            seen_digit = true;
        }
        if (!seen_digit) {
            return std::numeric_limits<f64>::quiet_NaN();
        }
        try {
            return std::stod(str.std_string());
        } catch (...) {
            return std::numeric_limits<f64>::quiet_NaN();
        }
    }
    if (is_object()) {
        if (auto* date = dynamic_cast<DateObject*>(as_object())) {
            return date->time_value();
        }
        return std::numeric_limits<f64>::quiet_NaN();
    }
    return std::numeric_limits<f64>::quiet_NaN();
}

i32 Value::to_int32() const {
    f64 num = to_number();
    if (std::isnan(num) || std::isinf(num) || num == 0) {
        return 0;
    }
    i64 int64 = static_cast<i64>(std::trunc(num));
    return static_cast<i32>(int64);
}

u32 Value::to_uint32() const {
    f64 num = to_number();
    if (std::isnan(num) || std::isinf(num) || num == 0) {
        return 0;
    }
    i64 int64 = static_cast<i64>(std::trunc(num));
    return static_cast<u32>(int64);
}

String Value::to_string() const {
    if (is_undefined()) return "undefined"_s;
    if (is_null()) return "null"_s;
    if (is_boolean()) return as_boolean() ? "true"_s : "false"_s;
    if (is_number()) {
        f64 n = as_number();
        if (std::isnan(n)) return "NaN"_s;
        if (std::isinf(n)) return n > 0 ? "Infinity"_s : "-Infinity"_s;
        std::ostringstream oss;
        oss << n;
        return String(oss.str());
    }
    if (is_string()) return as_string();
    if (is_object()) {
        if (auto* date = dynamic_cast<DateObject*>(as_object())) {
            return date->string_value();
        }
        return "[object Object]"_s;
    }
    return ""_s;
}

// ============================================================================
// Comparison
// ============================================================================

bool Value::strict_equals(const Value& other) const {
    // Fast path: identical bit patterns
    if (m_bits == other.m_bits) {
        // But NaN !== NaN
        if (is_double()) {
            f64 n = as_number();
            return !std::isnan(n);
        }
        return true;
    }

    // Different types can't be strictly equal (with one exception: integer vs double)
    if (type() != other.type()) {
        // Check if one is integer and other is double with same value
        if (is_number() && other.is_number()) {
            return as_number() == other.as_number();
        }
        return false;
    }

    // Same type, different bits
    switch (type()) {
        case ValueType::Number: {
            f64 a = as_number();
            f64 b = other.as_number();
            if (std::isnan(a) || std::isnan(b)) return false;
            return a == b;
        }
        case ValueType::String: {
            // Fast path: interned strings can be compared by pointer
            auto* s1 = get_pointer<InternedString>();
            auto* s2 = other.get_pointer<InternedString>();
            if (s1 == s2) return true;  // Same interned string
            // Fallback to value comparison (shouldn't happen with interning)
            return s1 && s2 && s1->str == s2->str;
        }
        case ValueType::Object:
            return as_object() == other.as_object();
        default:
            return false;
    }
}

bool Value::loose_equals(const Value& other) const {
    if (type() == other.type()) {
        return strict_equals(other);
    }

    if ((is_null() && other.is_undefined()) || (is_undefined() && other.is_null())) {
        return true;
    }

    if (is_number() && other.is_string()) {
        return as_number() == other.to_number();
    }
    if (is_string() && other.is_number()) {
        return to_number() == other.as_number();
    }

    if (is_boolean()) {
        return Value(to_number()).loose_equals(other);
    }
    if (other.is_boolean()) {
        return loose_equals(Value(other.to_number()));
    }

    if (is_object() && (other.is_string() || other.is_number())) {
        return Value(to_string()).loose_equals(other);
    }
    if ((is_string() || is_number()) && other.is_object()) {
        return loose_equals(Value(other.to_string()));
    }

    return false;
}

// ============================================================================
// Type name
// ============================================================================

String Value::typeof_string() const {
    if (is_undefined()) return "undefined"_s;
    if (is_null()) return "object"_s;
    if (is_boolean()) return "boolean"_s;
    if (is_number()) return "number"_s;
    if (is_string()) return "string"_s;
    if (is_object()) {
        if (is_function()) return "function"_s;
        return "object"_s;
    }
    return "undefined"_s;
}

// ============================================================================
// Debug
// ============================================================================

String Value::debug_string() const {
    std::ostringstream oss;
    if (is_undefined()) {
        oss << "undefined";
    } else if (is_null()) {
        oss << "null";
    } else if (is_boolean()) {
        oss << (as_boolean() ? "true" : "false");
    } else if (is_number()) {
        oss << as_number();
    } else if (is_string()) {
        oss << "\"" << as_string().data() << "\"";
    } else if (is_object()) {
        if (dynamic_cast<DateObject*>(as_object())) {
            oss << "[Date " << as_number() << "]";
        } else if (is_function()) {
            oss << "[Function]";
        } else if (is_array()) {
            oss << "[Array]";
        } else {
            oss << "[Object]";
        }
    } else {
        oss << "[unknown]";
    }
    return String(oss.str());
}

void Value::mark() const {
    if (is_object()) {
        auto* obj = as_object();
        if (obj) obj->mark();
    }
}

Value Value::native_function(NativeFn fn, u8 arity, const String& name) {
    // DEPRECATED: This function cannot work with GC - the object would be immediately collected
    // Use VM::define_native instead, which allocates through the GC
    // Returning undefined as a safe fallback
    (void)fn;
    (void)arity;
    (void)name;
    return Value::undefined();
}

// ============================================================================
// Value operations
// ============================================================================

namespace value_ops {

Value add(const Value& lhs, const Value& rhs) {
    // JavaScript + operator: if either operand is a string, concatenate
    // Otherwise, convert both to numbers and add
    if (lhs.is_string() || rhs.is_string()) {
        return Value(lhs.to_string() + rhs.to_string());
    }
    return Value(lhs.to_number() + rhs.to_number());
}

Value subtract(const Value& lhs, const Value& rhs) {
    return Value(lhs.to_number() - rhs.to_number());
}

Value multiply(const Value& lhs, const Value& rhs) {
    return Value(lhs.to_number() * rhs.to_number());
}

Value divide(const Value& lhs, const Value& rhs) {
    return Value(lhs.to_number() / rhs.to_number());
}

Value modulo(const Value& lhs, const Value& rhs) {
    return Value(std::fmod(lhs.to_number(), rhs.to_number()));
}

Value exponent(const Value& lhs, const Value& rhs) {
    return Value(std::pow(lhs.to_number(), rhs.to_number()));
}

Value negate(const Value& val) {
    return Value(-val.to_number());
}

Value bitwise_not(const Value& val) {
    return Value(static_cast<f64>(~val.to_int32()));
}

Value bitwise_and(const Value& lhs, const Value& rhs) {
    return Value(static_cast<f64>(lhs.to_int32() & rhs.to_int32()));
}

Value bitwise_or(const Value& lhs, const Value& rhs) {
    return Value(static_cast<f64>(lhs.to_int32() | rhs.to_int32()));
}

Value bitwise_xor(const Value& lhs, const Value& rhs) {
    return Value(static_cast<f64>(lhs.to_int32() ^ rhs.to_int32()));
}

Value left_shift(const Value& lhs, const Value& rhs) {
    u32 shift = rhs.to_uint32() & 0x1F;
    return Value(static_cast<f64>(lhs.to_int32() << shift));
}

Value right_shift(const Value& lhs, const Value& rhs) {
    u32 shift = rhs.to_uint32() & 0x1F;
    return Value(static_cast<f64>(lhs.to_int32() >> shift));
}

Value unsigned_right_shift(const Value& lhs, const Value& rhs) {
    u32 shift = rhs.to_uint32() & 0x1F;
    return Value(static_cast<f64>(lhs.to_uint32() >> shift));
}

Value less_than(const Value& lhs, const Value& rhs) {
    if (lhs.is_string() && rhs.is_string()) {
        return Value(lhs.as_string().std_string() < rhs.as_string().std_string());
    }
    f64 l = lhs.to_number();
    f64 r = rhs.to_number();
    if (std::isnan(l) || std::isnan(r)) {
        return Value(false);
    }
    return Value(l < r);
}

Value less_equal(const Value& lhs, const Value& rhs) {
    if (lhs.is_string() && rhs.is_string()) {
        return Value(lhs.as_string().std_string() <= rhs.as_string().std_string());
    }
    f64 l = lhs.to_number();
    f64 r = rhs.to_number();
    if (std::isnan(l) || std::isnan(r)) {
        return Value(false);
    }
    return Value(l <= r);
}

Value greater_than(const Value& lhs, const Value& rhs) {
    if (lhs.is_string() && rhs.is_string()) {
        return Value(lhs.as_string().std_string() > rhs.as_string().std_string());
    }
    f64 l = lhs.to_number();
    f64 r = rhs.to_number();
    if (std::isnan(l) || std::isnan(r)) {
        return Value(false);
    }
    return Value(l > r);
}

Value greater_equal(const Value& lhs, const Value& rhs) {
    if (lhs.is_string() && rhs.is_string()) {
        return Value(lhs.as_string().std_string() >= rhs.as_string().std_string());
    }
    f64 l = lhs.to_number();
    f64 r = rhs.to_number();
    if (std::isnan(l) || std::isnan(r)) {
        return Value(false);
    }
    return Value(l >= r);
}

Value logical_not(const Value& val) {
    return Value(!val.to_boolean());
}

Value typeof_op(const Value& val) {
    (void)val;
    return Value::undefined();
}

Value instanceof_op(const Value& obj, const Value& constructor) {
    if (!obj.is_object() || !constructor.is_object()) {
        return Value(false);
    }
    auto* ctor_obj = constructor.as_object();
    Value proto_val = ctor_obj->get_property("prototype"_s);
    if (!proto_val.is_object()) {
        return Value(false);
    }
    Object* proto = proto_val.as_object();
    Object* current = obj.as_object();
    while (current) {
        if (current == proto) {
            return Value(true);
        }
        current = current->prototype();
    }
    return Value(false);
}

Value in_op(const Value& key, const Value& obj) {
    if (!obj.is_object()) {
        return Value(false);
    }
    String prop = key.to_string();
    return Value(obj.as_object()->has_property(prop));
}

} // namespace value_ops

} // namespace lithium::js
