/**
 * JavaScript Object implementation with Hidden Classes (Shapes) and Inline Caching
 */

#include "lithium/js/object.hpp"
#include "lithium/js/vm.hpp"
#include "lithium/js/gc.hpp"
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <optional>
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
    // Check dynamic properties first (for Map.size, Set.size, etc.)
    if (has_dynamic_property(name)) {
        return get_dynamic_property(name);
    }

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

// Fast property access with polymorphic inline caching
Value Object::get_property_cached(const String& name, InlineCacheEntry& cache) const {
    // Polymorphic IC fast path: check if current shape is in cache
    if (cache.valid && m_shape) {
        i32 cached_slot = cache.find_slot(m_shape->id());
        if (cached_slot >= 0 && static_cast<usize>(cached_slot) < m_slots.size()) {
            // Cache hit - direct slot access
            return m_slots[cached_slot];
        }
    }

    // Cache miss - look up property in shape
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0 && static_cast<usize>(slot) < m_slots.size()) {
            // Add this shape to the polymorphic cache
            cache.add_shape(m_shape->id(), slot);
            return m_slots[slot];
        }
    }

    // Property not in shape - check overflow and prototype
    // Don't cache these (too complex to track)
    return get_property(name);
}

void Object::set_property_cached(const String& name, const Value& value, InlineCacheEntry& cache) {
    // Polymorphic IC fast path: check if current shape is in cache
    if (cache.valid && m_shape) {
        i32 cached_slot = cache.find_slot(m_shape->id());
        if (cached_slot >= 0 && static_cast<usize>(cached_slot) < m_slots.size()) {
            // Cache hit - direct slot update
            m_slots[cached_slot] = value;
            return;
        }
    }

    // Cache miss - need to go through slow path (may transition shape)
    set_property(name, value);

    // After set_property, update cache with new shape info
    if (m_shape) {
        i32 slot = m_shape->find_slot(name);
        if (slot >= 0) {
            cache.add_shape(m_shape->id(), slot);
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

void Object::trace(GarbageCollector& gc) {
    // Mark slot values using GC to properly add them to gray stack
    for (auto& slot : m_slots) {
        gc.mark_value(slot);
    }
    // Mark overflow properties
    for (auto& [_, value] : m_overflow_properties) {
        gc.mark_value(value);
    }
    // Mark prototype
    if (m_prototype) {
        gc.mark_object(m_prototype.get());
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

void BoundFunction::trace(GarbageCollector& gc) {
    Object::trace(gc);
    gc.mark_value(m_target);
    gc.mark_value(m_receiver);
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

void Array::trace(GarbageCollector& gc) {
    Object::trace(gc);
    for (auto& element : m_elements) {
        gc.mark_value(element);
    }
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

// ============================================================================
// Map
// ============================================================================

MapObject::MapObject() = default;

// SameValueZero equality check for Map/Set (like === but NaN === NaN)
static bool same_value_zero(const Value& a, const Value& b) {
    // Handle NaN case (NaN should equal NaN in SameValueZero)
    if (a.is_number() && b.is_number()) {
        f64 a_num = a.to_number();
        f64 b_num = b.to_number();
        if (std::isnan(a_num) && std::isnan(b_num)) {
            return true;
        }
        return a_num == b_num;
    }

    // For objects, compare by identity (same reference)
    if (a.is_object() && b.is_object()) {
        return a.as_object() == b.as_object();
    }

    // For other types, use strict equality
    return a.strict_equals(b);
}

std::optional<usize> MapObject::find_entry(const Value& key) const {
    for (usize i = 0; i < m_entries.size(); ++i) {
        if (same_value_zero(m_entries[i].key, key)) {
            return i;
        }
    }
    return std::nullopt;
}

void MapObject::set(const Value& key, const Value& value) {
    auto idx = find_entry(key);
    if (idx.has_value()) {
        m_entries[idx.value()].value = value;
    } else {
        m_entries.push_back({key, value});
    }
}

Value MapObject::get(const Value& key) const {
    auto idx = find_entry(key);
    if (idx.has_value()) {
        return m_entries[idx.value()].value;
    }
    return Value::undefined();
}

bool MapObject::has(const Value& key) const {
    return find_entry(key).has_value();
}

bool MapObject::remove(const Value& key) {
    auto idx = find_entry(key);
    if (idx.has_value()) {
        m_entries.erase(m_entries.begin() + idx.value());
        return true;
    }
    return false;
}

void MapObject::clear() {
    m_entries.clear();
}

void MapObject::trace(GarbageCollector& gc) {
    Object::trace(gc);
    for (auto& entry : m_entries) {
        gc.mark_value(entry.key);
        gc.mark_value(entry.value);
    }
}

bool MapObject::has_dynamic_property(const String& name) const {
    return name == "size"_s;
}

Value MapObject::get_dynamic_property(const String& name) const {
    if (name == "size"_s) {
        return Value(static_cast<f64>(m_entries.size()));
    }
    return Value::undefined();
}

// ============================================================================
// Set
// ============================================================================

SetObject::SetObject() = default;

std::optional<usize> SetObject::find_value(const Value& value) const {
    for (usize i = 0; i < m_values.size(); ++i) {
        if (same_value_zero(m_values[i], value)) {
            return i;
        }
    }
    return std::nullopt;
}

void SetObject::add(const Value& value) {
    if (!find_value(value).has_value()) {
        m_values.push_back(value);
    }
}

bool SetObject::has(const Value& value) const {
    return find_value(value).has_value();
}

bool SetObject::remove(const Value& value) {
    auto idx = find_value(value);
    if (idx.has_value()) {
        m_values.erase(m_values.begin() + idx.value());
        return true;
    }
    return false;
}

void SetObject::clear() {
    m_values.clear();
}

void SetObject::trace(GarbageCollector& gc) {
    Object::trace(gc);
    for (auto& value : m_values) {
        gc.mark_value(value);
    }
}

bool SetObject::has_dynamic_property(const String& name) const {
    return name == "size"_s;
}

Value SetObject::get_dynamic_property(const String& name) const {
    if (name == "size"_s) {
        return Value(static_cast<f64>(m_values.size()));
    }
    return Value::undefined();
}

// ============================================================================
// WeakMap
// ============================================================================

WeakMapObject::WeakMapObject() = default;

void WeakMapObject::set(const Value& key, const Value& value) {
    if (!key.is_object()) {
        // WeakMap keys must be objects
        return;
    }
    m_entries[key.as_object()] = value;
}

Value WeakMapObject::get(const Value& key) const {
    if (!key.is_object()) {
        return Value::undefined();
    }
    auto it = m_entries.find(key.as_object());
    if (it != m_entries.end()) {
        return it->second;
    }
    return Value::undefined();
}

bool WeakMapObject::has(const Value& key) const {
    if (!key.is_object()) {
        return false;
    }
    return m_entries.find(key.as_object()) != m_entries.end();
}

bool WeakMapObject::remove(const Value& key) {
    if (!key.is_object()) {
        return false;
    }
    return m_entries.erase(key.as_object()) > 0;
}

void WeakMapObject::trace(GarbageCollector& gc) {
    Object::trace(gc);
    // Only trace values, not keys (weak references)
    for (auto& [key, value] : m_entries) {
        gc.mark_value(value);
    }
}

// ============================================================================
// WeakSet
// ============================================================================

WeakSetObject::WeakSetObject() = default;

void WeakSetObject::add(const Value& value) {
    if (!value.is_object()) {
        // WeakSet values must be objects
        return;
    }
    m_values.insert(value.as_object());
}

bool WeakSetObject::has(const Value& value) const {
    if (!value.is_object()) {
        return false;
    }
    return m_values.find(value.as_object()) != m_values.end();
}

bool WeakSetObject::remove(const Value& value) {
    if (!value.is_object()) {
        return false;
    }
    return m_values.erase(value.as_object()) > 0;
}

void WeakSetObject::trace(GarbageCollector& gc) {
    Object::trace(gc);
    // No need to trace values (weak references)
}

} // namespace lithium::js
