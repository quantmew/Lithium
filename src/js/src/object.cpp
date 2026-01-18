/**
 * JavaScript Object implementation with Hidden Classes (Shapes) and Inline Caching
 */

#include "lithium/js/object.hpp"
#include "lithium/js/vm.hpp"
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lithium::js {

// ============================================================================
// Object - Shape-based implementation
// ============================================================================

Object::Object() : m_shape(ShapeRegistry::instance().root_shape()) {}

bool Object::has_property(const String& name) const {
    // Check shape-based properties
    if (m_shape && m_shape->find_slot(name) >= 0) {
        return true;
    }
    // Check overflow properties
    if (m_overflow_properties.count(name) > 0) {
        return true;
    }
    // Check prototype
    if (m_prototype) {
        return m_prototype->has_property(name);
    }
    return false;
}

bool Object::has_own_property(const String& name) const {
    if (m_shape && m_shape->find_slot(name) >= 0) {
        return true;
    }
    return m_overflow_properties.count(name) > 0;
}

Value Object::get_property(const String& name) const {
    // Fast path: check shape-based slot
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0 && static_cast<usize>(slot) < m_slots.size()) {
            return m_slots[slot];
        }
    }
    // Check overflow properties
    auto it = m_overflow_properties.find(name);
    if (it != m_overflow_properties.end()) {
        return it->second;
    }
    // Check prototype
    if (m_prototype) {
        return m_prototype->get_property(name);
    }
    return Value::undefined();
}

void Object::set_property(const String& name, const Value& value) {
    // Check if property exists in current shape
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0) {
            // Property exists, just update the slot
            if (static_cast<usize>(slot) >= m_slots.size()) {
                m_slots.resize(slot + 1);
            }
            m_slots[slot] = value;
            return;
        }
    }

    // Property doesn't exist, need to transition to new shape
    if (m_shape) {
        ShapePtr new_shape = m_shape->add_property(name);
        i32 new_slot = new_shape->find_slot(name);
        if (new_slot >= 0) {
            m_shape = new_shape;
            if (static_cast<usize>(new_slot) >= m_slots.size()) {
                m_slots.resize(new_slot + 1);
            }
            m_slots[new_slot] = value;
            return;
        }
    }

    // Fallback to overflow properties
    m_overflow_properties[name] = value;
}

bool Object::delete_property(const String& name) {
    // For simplicity, move to overflow on delete (shapes don't support deletion well)
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0 && static_cast<usize>(slot) < m_slots.size()) {
            // Move remaining properties to overflow and reset shape
            auto prop_names = m_shape->property_names();
            for (usize i = 0; i < prop_names.size(); ++i) {
                if (prop_names[i] != name && i < m_slots.size()) {
                    m_overflow_properties[prop_names[i]] = m_slots[i];
                }
            }
            m_slots.clear();
            m_shape = ShapeRegistry::instance().root_shape();
            return true;
        }
    }
    return m_overflow_properties.erase(name) > 0;
}

// Fast property access with inline caching
Value Object::get_property_cached(const String& name, InlineCacheEntry& cache) const {
    // IC fast path: if shape matches, use cached slot directly
    if (cache.valid && m_shape && cache.shape_id == m_shape->id()) {
        if (cache.slot >= 0 && static_cast<usize>(cache.slot) < m_slots.size()) {
            return m_slots[cache.slot];
        }
    }

    // Slow path: look up property and update cache
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0 && static_cast<usize>(slot) < m_slots.size()) {
            // Update cache
            cache.shape_id = m_shape->id();
            cache.slot = slot;
            cache.valid = true;
            return m_slots[slot];
        }
    }

    // Not in shape, invalidate cache and use slow path
    cache.invalidate();
    return get_property(name);
}

void Object::set_property_cached(const String& name, const Value& value, InlineCacheEntry& cache) {
    // IC fast path: if shape matches and property exists, use cached slot
    if (cache.valid && m_shape && cache.shape_id == m_shape->id()) {
        if (cache.slot >= 0 && static_cast<usize>(cache.slot) < m_slots.size()) {
            m_slots[cache.slot] = value;
            return;
        }
    }

    // Need to go through slow path (may transition shape)
    // After set_property, update cache if property is now in shape
    set_property(name, value);

    // Update cache with new shape info
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0) {
            cache.shape_id = m_shape->id();
            cache.slot = slot;
            cache.valid = true;
        }
    }
}

// Direct slot access (for IC fast path)
Value Object::get_slot(u32 slot) const {
    if (slot < m_slots.size()) {
        return m_slots[slot];
    }
    return Value::undefined();
}

void Object::set_slot(u32 slot, const Value& value) {
    if (slot >= m_slots.size()) {
        m_slots.resize(slot + 1);
    }
    m_slots[slot] = value;
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

    // Get shape-based property names
    if (m_shape) {
        auto shape_names = m_shape->property_names();
        names.insert(names.end(), shape_names.begin(), shape_names.end());
    }

    // Add overflow property names
    for (const auto& [key, _] : m_overflow_properties) {
        names.push_back(key);
    }

    return names;
}

void Object::trace() {
    // Mark slot values
    for (auto& slot : m_slots) {
        slot.mark();
    }
    // Mark overflow properties
    for (auto& [_, value] : m_overflow_properties) {
        value.mark();
    }
    // Mark prototype
    if (m_prototype) {
        m_prototype->mark();
    }
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
    Object::trace();
}

// ============================================================================
// DateObject
// ============================================================================

DateObject::DateObject() = default;

DateObject::DateObject(f64 ms_since_epoch)
    : m_time(ms_since_epoch) {}

String DateObject::string_value() const {
    if (std::isnan(m_time) || std::isinf(m_time)) {
        return "Invalid Date"_s;
    }

    using namespace std::chrono;

    auto millis = duration_cast<milliseconds>(milliseconds(static_cast<long long>(m_time)));
    auto tp = system_clock::time_point(millis);
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    auto ms = static_cast<long long>(m_time) % 1000;
    if (ms < 0) {
        ms += 1000;
    }
    oss << "." << std::setw(3) << std::setfill('0') << ms;
    return String(oss.str());
}

} // namespace lithium::js
