/**
 * JavaScript Object implementation
 */

#include "lithium/js/object.hpp"
#include "lithium/js/vm.hpp"

namespace lithium::js {

// ============================================================================
// Object implementation
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

void Object::trace() {
    for (auto& [_, value] : m_properties) {
        value.mark();
    }
    if (m_prototype) {
        m_prototype->mark();
    }
}

// ============================================================================
// Closure implementation
// ============================================================================

Closure::Closure(std::shared_ptr<Function> function)
    : m_function(std::move(function))
{
    m_upvalues.resize(m_function->upvalue_count());
}

void Closure::set_upvalue(usize index, std::shared_ptr<UpvalueSlot> upvalue) {
    if (index < m_upvalues.size()) {
        m_upvalues[index] = std::move(upvalue);
    }
}

UpvalueSlot* Closure::get_upvalue(usize index) const {
    if (index < m_upvalues.size()) {
        return m_upvalues[index].get();
    }
    return nullptr;
}

void Closure::trace() {
    Object::trace();
    for (auto& upvalue : m_upvalues) {
        if (upvalue && !upvalue->is_open) {
            upvalue->closed.mark();
        }
    }
}

// ============================================================================
// NativeFunction implementation
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
// Array implementation
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

    // Check if it's a numeric index
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

    // Check if it's a numeric index
    try {
        usize index = std::stoull(std::string(name.data(), name.size()));
        set_element(static_cast<u32>(index), value);
        return;
    } catch (...) {}

    Object::set_property(name, value);
}

void Array::trace() {
    Object::trace();
    for (auto& element : m_elements) {
        element.mark();
    }
}

// ============================================================================
// Class implementation
// ============================================================================

Class::Class(String name) : m_name(std::move(name)) {}

void Class::set_method(const String& name, std::shared_ptr<Closure> method) {
    m_methods[name] = std::move(method);
}

Closure* Class::get_method(const String& name) const {
    auto it = m_methods.find(name);
    if (it != m_methods.end()) {
        return it->second.get();
    }
    if (m_superclass) {
        return m_superclass->get_method(name);
    }
    return nullptr;
}

bool Class::has_method(const String& name) const {
    if (m_methods.count(name) > 0) {
        return true;
    }
    if (m_superclass) {
        return m_superclass->has_method(name);
    }
    return false;
}

void Class::set_static_method(const String& name, std::shared_ptr<Closure> method) {
    m_static_methods[name] = std::move(method);
}

Closure* Class::get_static_method(const String& name) const {
    auto it = m_static_methods.find(name);
    if (it != m_static_methods.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Class::set_superclass(std::shared_ptr<Class> superclass) {
    m_superclass = std::move(superclass);
}

void Class::trace() {
    Object::trace();
    for (auto& [_, method] : m_methods) {
        if (method) method->mark();
    }
    for (auto& [_, method] : m_static_methods) {
        if (method) method->mark();
    }
    if (m_superclass) {
        m_superclass->mark();
    }
}

// ============================================================================
// Instance implementation
// ============================================================================

Instance::Instance(std::shared_ptr<Class> klass)
    : m_class(std::move(klass))
{
    set_prototype(m_class);
}

void Instance::trace() {
    Object::trace();
    if (m_class) {
        m_class->mark();
    }
}

// ============================================================================
// BoundMethod implementation
// ============================================================================

BoundMethod::BoundMethod(Value receiver, std::shared_ptr<Closure> method)
    : m_receiver(std::move(receiver))
    , m_method(std::move(method))
{
}

void BoundMethod::trace() {
    Object::trace();
    m_receiver.mark();
    if (m_method) {
        m_method->mark();
    }
}

} // namespace lithium::js
