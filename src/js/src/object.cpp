/**
 * JavaScript Object implementation (minimal)
 */

#include "lithium/js/object.hpp"
#include "lithium/js/vm.hpp"
#include <cmath>

namespace lithium::js {

// ============================================================================
// Object
// ============================================================================

bool Object::has_property(const String& name) const {
    if (m_properties.count(name) > 0) {
        return true;
    }
    if (m_prototype) {
        return m_prototype->has_property(name);
    }
    return false;
}

Value Object::get_property(const String& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return it->second;
    }
    if (m_prototype) {
        return m_prototype->get_property(name);
    }
    return Value::undefined();
}

void Object::set_property(const String& name, const Value& value) {
    m_properties[name] = value;
}

bool Object::delete_property(const String& name) {
    return m_properties.erase(name) > 0;
}

bool Object::has_element(u32 index) const {
    return has_property(String(std::to_string(index)));
}

Value Object::get_element(u32 index) const {
    return get_property(String(std::to_string(index)));
}

void Object::set_element(u32 index, const Value& value) {
    set_property(String(std::to_string(index)), value);
}

bool Object::delete_element(u32 index) {
    return delete_property(String(std::to_string(index)));
}

std::vector<String> Object::own_property_names() const {
    std::vector<String> names;
    names.reserve(m_properties.size());
    for (const auto& [key, _] : m_properties) {
        names.push_back(key);
    }
    return names;
}

// ============================================================================
// NativeFunction
// ============================================================================

NativeFunction::NativeFunction(String name, NativeFn fn, u8 arity)
    : m_name(std::move(name))
    , m_function(std::move(fn))
    , m_arity(arity)
{
}

Value NativeFunction::call(VM& vm, const std::vector<Value>& args) {
    return m_function(vm, args);
}

// ============================================================================
// BoundFunction
// ============================================================================

BoundFunction::BoundFunction(Value target, Value receiver)
    : m_target(std::move(target))
    , m_receiver(std::move(receiver)) {
}

void BoundFunction::trace() {
    m_target.mark();
    m_receiver.mark();
}

// ============================================================================
// Array
// ============================================================================

Array::Array() = default;

Array::Array(usize initial_size) : m_elements(initial_size) {}

Array::Array(std::initializer_list<Value> values) : m_elements(values) {}

void Array::set_length(usize len) {
    m_elements.resize(len);
}

void Array::push(const Value& value) {
    m_elements.push_back(value);
}

Value Array::pop() {
    if (m_elements.empty()) {
        return Value::undefined();
    }
    Value value = m_elements.back();
    m_elements.pop_back();
    return value;
}

Value Array::shift() {
    if (m_elements.empty()) {
        return Value::undefined();
    }
    Value value = m_elements.front();
    m_elements.erase(m_elements.begin());
    return value;
}

void Array::unshift(const Value& value) {
    m_elements.insert(m_elements.begin(), value);
}

bool Array::has_element(u32 index) const {
    return index < m_elements.size();
}

Value Array::get_element(u32 index) const {
    if (index >= m_elements.size()) {
        return Value::undefined();
    }
    return m_elements[index];
}

void Array::set_element(u32 index, const Value& value) {
    if (index >= m_elements.size()) {
        m_elements.resize(index + 1);
    }
    m_elements[index] = value;
}

bool Array::delete_element(u32 index) {
    if (index >= m_elements.size()) {
        return false;
    }
    m_elements[index] = Value::undefined();
    return true;
}

Value Array::get_property(const String& name) const {
    if (name == "length"_s) {
        return Value(static_cast<f64>(m_elements.size()));
    }

    try {
        usize index = std::stoull(std::string(name.data(), name.size()));
        if (index < m_elements.size()) {
            return m_elements[index];
        }
    } catch (...) {}

    return Object::get_property(name);
}

void Array::set_property(const String& name, const Value& value) {
    if (name == "length"_s) {
        f64 len = value.to_number();
        if (len >= 0 && len == std::floor(len)) {
            m_elements.resize(static_cast<usize>(len));
        }
        return;
    }

    try {
        usize index = std::stoull(std::string(name.data(), name.size()));
        set_element(static_cast<u32>(index), value);
        return;
    } catch (...) {}

    Object::set_property(name, value);
}

void Array::trace() {
    for (auto& element : m_elements) {
        element.mark();
    }
    if (m_prototype) {
        m_prototype->mark();
    }
    for (auto& [_, value] : m_properties) {
        value.mark();
    }
}

} // namespace lithium::js
