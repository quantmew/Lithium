/**
 * JavaScript Value implementation
 */

#include "lithium/js/value.hpp"
#include "lithium/js/object.hpp"
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

namespace lithium::js {

// ============================================================================
// Value constructors
// ============================================================================

Value::Value() : m_type(ValueType::Undefined) {}

Value::Value(std::nullptr_t) : m_type(ValueType::Null) {}

Value::Value(bool b) : m_type(ValueType::Boolean) {
    m_primitive.boolean = b;
}

Value::Value(f64 n) : m_type(ValueType::Number) {
    m_primitive.number = n;
}

Value::Value(i32 n) : m_type(ValueType::Number) {
    m_primitive.number = static_cast<f64>(n);
}

Value::Value(const char* s) : m_type(ValueType::String) {
    m_string = std::make_shared<String>(s);
}

Value::Value(const String& s) : m_type(ValueType::String) {
    m_string = std::make_shared<String>(s);
}

Value::Value(Object* obj) : m_type(ValueType::Object) {
    m_object = std::shared_ptr<Object>(obj, [](Object*) {});
}

Value::Value(std::shared_ptr<Object> obj) : m_type(ValueType::Object) {
    m_object = std::move(obj);
}

// ============================================================================
// Type checking
// ============================================================================

bool Value::is_function() const {
    return is_callable();
}

bool Value::is_callable() const {
    return is_object() && m_object && m_object->is_callable();
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

bool Value::as_boolean() const {
    if (m_type != ValueType::Boolean) {
        return false;
    }
    return m_primitive.boolean;
}

f64 Value::as_number() const {
    if (m_type != ValueType::Number) {
        return std::numeric_limits<f64>::quiet_NaN();
    }
    return m_primitive.number;
}

const String& Value::as_string() const {
    static String empty;
    if (m_type != ValueType::String || !m_string) {
        return empty;
    }
    return *m_string;
}

Object* Value::as_object() const {
    if (m_type != ValueType::Object) {
        return nullptr;
    }
    return m_object.get();
}

NativeFunction* Value::as_native_function() const {
    return dynamic_cast<NativeFunction*>(as_object());
}

// ============================================================================
// Type conversion
// ============================================================================

bool Value::to_boolean() const {
    switch (m_type) {
        case ValueType::Undefined:
        case ValueType::Null:
            return false;
        case ValueType::Boolean:
            return m_primitive.boolean;
        case ValueType::Number:
            return m_primitive.number != 0 && !std::isnan(m_primitive.number);
        case ValueType::String:
            return m_string && !m_string->empty();
        case ValueType::Object:
            return m_object != nullptr;
        default:
            return false;
    }
}

f64 Value::to_number() const {
    switch (m_type) {
        case ValueType::Undefined:
            return std::numeric_limits<f64>::quiet_NaN();
        case ValueType::Null:
            return 0.0;
        case ValueType::Boolean:
            return m_primitive.boolean ? 1.0 : 0.0;
        case ValueType::Number:
            return m_primitive.number;
        case ValueType::String: {
            if (!m_string) {
                return std::numeric_limits<f64>::quiet_NaN();
            }
            const auto& str = *m_string;
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
        case ValueType::Object:
            return std::numeric_limits<f64>::quiet_NaN();
        default:
            return std::numeric_limits<f64>::quiet_NaN();
    }
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
    switch (m_type) {
        case ValueType::Undefined:
            return "undefined"_s;
        case ValueType::Null:
            return "null"_s;
        case ValueType::Boolean:
            return m_primitive.boolean ? "true"_s : "false"_s;
        case ValueType::Number: {
            if (std::isnan(m_primitive.number)) {
                return "NaN"_s;
            }
            if (std::isinf(m_primitive.number)) {
                return m_primitive.number > 0 ? "Infinity"_s : "-Infinity"_s;
            }
            std::ostringstream oss;
            oss << m_primitive.number;
            return String(oss.str());
        }
        case ValueType::String:
            return m_string ? *m_string : String();
        case ValueType::Object:
            return "[object Object]"_s;
        default:
            return ""_s;
    }
}

// ============================================================================
// Comparison
// ============================================================================

bool Value::strict_equals(const Value& other) const {
    if (m_type != other.m_type) {
        return false;
    }

    switch (m_type) {
        case ValueType::Undefined:
        case ValueType::Null:
            return true;
        case ValueType::Boolean:
            return m_primitive.boolean == other.m_primitive.boolean;
        case ValueType::Number: {
            if (std::isnan(m_primitive.number) || std::isnan(other.m_primitive.number)) {
                return false;
            }
            return m_primitive.number == other.m_primitive.number;
        }
        case ValueType::String:
            return m_string && other.m_string && *m_string == *other.m_string;
        case ValueType::Object:
            return m_object.get() == other.m_object.get();
        default:
            return false;
    }
}

bool Value::loose_equals(const Value& other) const {
    if (m_type == other.m_type) {
        return strict_equals(other);
    }

    if ((is_null() && other.is_undefined()) || (is_undefined() && other.is_null())) {
        return true;
    }

    if (is_number() && other.is_string()) {
        return m_primitive.number == other.to_number();
    }
    if (is_string() && other.is_number()) {
        return to_number() == other.m_primitive.number;
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
    switch (m_type) {
        case ValueType::Undefined:
            return "undefined"_s;
        case ValueType::Null:
            return "object"_s;
        case ValueType::Boolean:
            return "boolean"_s;
        case ValueType::Number:
            return "number"_s;
        case ValueType::String:
            return "string"_s;
        case ValueType::Object:
            if (is_function()) {
                return "function"_s;
            }
            return "object"_s;
        default:
            return "undefined"_s;
    }
}

// ============================================================================
// Debug
// ============================================================================

String Value::debug_string() const {
    std::ostringstream oss;
    switch (m_type) {
        case ValueType::Undefined:
            oss << "undefined";
            break;
        case ValueType::Null:
            oss << "null";
            break;
        case ValueType::Boolean:
            oss << (m_primitive.boolean ? "true" : "false");
            break;
        case ValueType::Number:
            oss << m_primitive.number;
            break;
        case ValueType::String:
            oss << "\"" << (m_string ? m_string->data() : "") << "\"";
            break;
        case ValueType::Object:
            if (is_function()) {
                oss << "[Function]";
            } else if (is_array()) {
                oss << "[Array]";
            } else {
                oss << "[Object]";
            }
            break;
        default:
            oss << "[unknown]";
    }
    return String(oss.str());
}

void Value::mark() const {
    if (is_object() && m_object) {
        m_object->mark();
    }
}

Value Value::native_function(NativeFn fn, u8 arity, const String& name) {
    return Value(std::make_shared<NativeFunction>(name, std::move(fn), arity));
}

// ============================================================================
// Value operations
// ============================================================================

namespace value_ops {

Value add(const Value& lhs, const Value& rhs) {
    return Value(lhs.to_string() + rhs.to_string());
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

Value instanceof_op(const Value& /*obj*/, const Value& /*constructor*/) {
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
