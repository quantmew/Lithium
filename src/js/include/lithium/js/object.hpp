#pragma once

#include "value.hpp"
#include <functional>
#include <unordered_map>

namespace lithium::js {

class VM;

// ============================================================================
// Object - Base class for all JS objects
// ============================================================================

class Object {
public:
    virtual ~Object() = default;

    // Property access
    [[nodiscard]] virtual bool has_property(const String& name) const;
    [[nodiscard]] virtual Value get_property(const String& name) const;
    virtual void set_property(const String& name, const Value& value);
    virtual bool delete_property(const String& name);

    // Element access (for arrays)
    [[nodiscard]] virtual bool has_element(u32 index) const;
    [[nodiscard]] virtual Value get_element(u32 index) const;
    virtual void set_element(u32 index, const Value& value);
    virtual bool delete_element(u32 index);

    // Enumeration
    [[nodiscard]] virtual std::vector<String> own_property_names() const;

    // Prototype
    [[nodiscard]] Object* prototype() const { return m_prototype.get(); }
    void set_prototype(std::shared_ptr<Object> proto) { m_prototype = std::move(proto); }

    // Type identification
    [[nodiscard]] virtual bool is_callable() const { return false; }
    [[nodiscard]] virtual bool is_array() const { return false; }

    // GC stubs (no-op)
    void mark() { m_marked = true; }
    void unmark() { m_marked = false; }
    [[nodiscard]] bool is_marked() const { return m_marked; }
    virtual void trace() {}

protected:
    std::unordered_map<String, Value> m_properties;
    std::shared_ptr<Object> m_prototype;
    bool m_marked{false};
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

    void trace() override;

private:
    std::vector<Value> m_elements;
};

} // namespace lithium::js
