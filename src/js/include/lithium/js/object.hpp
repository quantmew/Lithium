#pragma once

#include "value.hpp"
#include "shape.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace lithium::js {

class VM;
class BoundFunction;

// ============================================================================
// Polymorphic Inline Cache Entry - cached property access for multiple shapes
// ============================================================================
//
// Caches up to 4 different shapes at each property access site.
// This handles the common case where a property is accessed on objects
// with 2-4 different shapes (e.g., in polymorphic code).
//
// Benefits over monomorphic IC:
// - Handles polymorphic code without constant cache invalidation
// - Still O(1) lookup for up to 4 shapes
// - Falls back to slow path only for megamorphic sites (>4 shapes)

struct InlineCacheEntry {
    static constexpr u32 MAX_SHAPES = 4;

    struct ShapeSlot {
        u32 shape_id{0};
        i32 slot{-1};

        [[nodiscard]] bool is_valid() const { return slot >= 0; }
        void clear() { shape_id = 0; slot = -1; }
    };

    ShapeSlot shapes[MAX_SHAPES];
    u8 next_slot{0};      // Round-robin index for adding new shapes
    bool valid{false};    // Is any entry valid?

    // Find cached slot for a shape, returns -1 if not found
    [[nodiscard]] i32 find_slot(u32 shape_id) const {
        for (u32 i = 0; i < MAX_SHAPES; ++i) {
            if (shapes[i].shape_id == shape_id && shapes[i].slot >= 0) {
                return shapes[i].slot;
            }
        }
        return -1;
    }

    // Add a new shape->slot mapping (round-robin replacement)
    void add_shape(u32 shape_id, i32 slot) {
        shapes[next_slot] = {shape_id, slot};
        next_slot = static_cast<u8>((next_slot + 1) % MAX_SHAPES);
        valid = true;
    }

    void invalidate() {
        for (auto& s : shapes) {
            s.clear();
        }
        next_slot = 0;
        valid = false;
    }
};

// ============================================================================
// Object - Base class for all JS objects (with Shape-based property storage)
// ============================================================================

class Object {
public:
    Object();
    virtual ~Object() = default;

    // Property access (slow path - for compatibility)
    [[nodiscard]] virtual bool has_property(const String& name) const;
    [[nodiscard]] virtual Value get_property(const String& name) const;
    virtual void set_property(const String& name, const Value& value);
    virtual bool delete_property(const String& name);
    [[nodiscard]] virtual bool has_own_property(const String& name) const;

    // Fast property access with inline caching
    [[nodiscard]] Value get_property_cached(const String& name, InlineCacheEntry& cache) const;
    void set_property_cached(const String& name, const Value& value, InlineCacheEntry& cache);

    // Direct slot access (for IC fast path)
    [[nodiscard]] Value get_slot(u32 slot) const;
    void set_slot(u32 slot, const Value& value);

    // Shape access for IC
    [[nodiscard]] u32 shape_id() const { return m_shape ? m_shape->id() : 0; }
    [[nodiscard]] ShapePtr shape() const { return m_shape; }

    // Element access (for arrays)
    [[nodiscard]] virtual bool has_element(u32 index) const;
    [[nodiscard]] virtual Value get_element(u32 index) const;
    virtual void set_element(u32 index, const Value& value);
    virtual bool delete_element(u32 index);

    // Dynamic property support (for Map.size, Set.size, etc.)
    [[nodiscard]] virtual bool has_dynamic_property(const String& name) const { (void)name; return false; }
    [[nodiscard]] virtual Value get_dynamic_property(const String& name) const { (void)name; return Value::undefined(); }

    // Enumeration
    [[nodiscard]] virtual std::vector<String> own_property_names() const;

    // Prototype
    [[nodiscard]] Object* prototype() const { return m_prototype.get(); }
    void set_prototype(std::shared_ptr<Object> proto) { m_prototype = std::move(proto); }

    // Type identification
    [[nodiscard]] virtual bool is_callable() const { return false; }
    [[nodiscard]] virtual bool is_array() const { return false; }

    // Stack allocation support (escape analysis)
    [[nodiscard]] bool is_stack_allocated() const { return m_stack_allocated; }
    void set_stack_allocated(bool val) { m_stack_allocated = val; }

    // GC support - mark and trace for garbage collection
    virtual void mark() { m_marked = true; }
    void unmark() { m_marked = false; }
    [[nodiscard]] bool is_marked() const { return m_marked; }

    // Trace object references - must be called with GC instance
    // to properly mark nested objects and add them to gray stack
    virtual void trace(class GarbageCollector& gc);

protected:
    // Shape-based property storage
    ShapePtr m_shape;
    std::vector<Value> m_slots;  // Dense property value storage

    // Fallback for deleted/special properties
    std::unordered_map<String, Value> m_overflow_properties;

    std::shared_ptr<Object> m_prototype;
    bool m_marked{false};
    bool m_stack_allocated{false};
};

// ============================================================================
// NativeFunction - Built-in function implemented in C++
// ============================================================================

class NativeFunction : public Object {
public:
    NativeFunction(String name, NativeFn fn, u8 arity = 0);

    [[nodiscard]] const String& name() const { return m_name; }
    [[nodiscard]] u8 arity() const { return m_arity; }
    [[nodiscard]] bool is_callable() const override { return true; }

    [[nodiscard]] Value call(VM& vm, const std::vector<Value>& args);

private:
    String m_name;
    NativeFn m_function;
    u8 m_arity;
};

// ============================================================================
// BoundFunction - callable that carries an explicit receiver
// ============================================================================

class BoundFunction : public Object {
public:
    BoundFunction(Value target, Value receiver);

    [[nodiscard]] Value target() const { return m_target; }
    [[nodiscard]] Value receiver() const { return m_receiver; }
    [[nodiscard]] bool is_callable() const override { return true; }

    void trace(GarbageCollector& gc) override;

private:
    Value m_target;
    Value m_receiver;
};

// ============================================================================
// Array
// ============================================================================

class Array : public Object {
public:
    Array();
    explicit Array(usize initial_size);
    Array(std::initializer_list<Value> values);

    [[nodiscard]] bool is_array() const override { return true; }

    // Array-specific operations
    [[nodiscard]] usize length() const { return m_elements.size(); }
    void set_length(usize len);

    void push(const Value& value);
    [[nodiscard]] Value pop();
    [[nodiscard]] Value shift();
    void unshift(const Value& value);

    // Override element access
    [[nodiscard]] bool has_element(u32 index) const override;
    [[nodiscard]] Value get_element(u32 index) const override;
    void set_element(u32 index, const Value& value) override;
    bool delete_element(u32 index) override;

    // Length property
    [[nodiscard]] Value get_property(const String& name) const override;
    void set_property(const String& name, const Value& value) override;

    void trace(GarbageCollector& gc) override;

private:
    std::vector<Value> m_elements;
};

// ============================================================================
// Date
// ============================================================================

class DateObject : public Object {
public:
    DateObject();
    explicit DateObject(f64 ms_since_epoch);

    [[nodiscard]] f64 time_value() const { return m_time; }
    void set_time_value(f64 ms) { m_time = ms; }
    [[nodiscard]] String string_value() const;

private:
    f64 m_time{0.0};
};

// ============================================================================
// Map - ES6 Map implementation (key-value pairs with any value as key)
// ============================================================================

class MapObject : public Object {
public:
    MapObject();

    struct Entry {
        Value key;
        Value value;
    };

    // Map operations
    void set(const Value& key, const Value& value);
    [[nodiscard]] Value get(const Value& key) const;
    [[nodiscard]] bool has(const Value& key) const;
    bool remove(const Value& key);
    void clear();
    [[nodiscard]] usize size() const { return m_entries.size(); }

    // Iteration support - returns internal entries for JS iteration
    [[nodiscard]] const std::vector<Entry>& internal_entries() const { return m_entries; }

    // Dynamic property support for 'size'
    [[nodiscard]] bool has_dynamic_property(const String& name) const override;
    [[nodiscard]] Value get_dynamic_property(const String& name) const override;

    void trace(GarbageCollector& gc) override;

private:
    std::vector<Entry> m_entries;

    // Find entry index by key (using SameValueZero equality)
    [[nodiscard]] std::optional<usize> find_entry(const Value& key) const;
};

// ============================================================================
// Set - ES6 Set implementation (unique values)
// ============================================================================

class SetObject : public Object {
public:
    SetObject();

    // Set operations
    void add(const Value& value);
    [[nodiscard]] bool has(const Value& value) const;
    bool remove(const Value& value);
    void clear();
    [[nodiscard]] usize size() const { return m_values.size(); }

    // Iteration support
    [[nodiscard]] const std::vector<Value>& values() const { return m_values; }

    // Dynamic property support for 'size'
    [[nodiscard]] bool has_dynamic_property(const String& name) const override;
    [[nodiscard]] Value get_dynamic_property(const String& name) const override;

    void trace(GarbageCollector& gc) override;

private:
    std::vector<Value> m_values;

    // Find value index (using SameValueZero equality)
    [[nodiscard]] std::optional<usize> find_value(const Value& value) const;
};

// ============================================================================
// WeakMap - ES6 WeakMap implementation (weak references to object keys)
// ============================================================================

class WeakMapObject : public Object {
public:
    WeakMapObject();

    // WeakMap operations (keys must be objects)
    void set(const Value& key, const Value& value);
    [[nodiscard]] Value get(const Value& key) const;
    [[nodiscard]] bool has(const Value& key) const;
    bool remove(const Value& key);

    void trace(GarbageCollector& gc) override;

private:
    // Use raw pointers for keys (weak references)
    std::unordered_map<Object*, Value> m_entries;
};

// ============================================================================
// WeakSet - ES6 WeakSet implementation (weak references to objects)
// ============================================================================

class WeakSetObject : public Object {
public:
    WeakSetObject();

    // WeakSet operations (values must be objects)
    void add(const Value& value);
    [[nodiscard]] bool has(const Value& value) const;
    bool remove(const Value& value);

    void trace(GarbageCollector& gc) override;

private:
    // Use raw pointers (weak references)
    std::unordered_set<Object*> m_values;
};

} // namespace lithium::js
