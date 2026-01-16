#pragma once

#include "value.hpp"
#include "bytecode.hpp"
#include <unordered_map>
#include <functional>

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
    [[nodiscard]] virtual bool is_class() const { return false; }

    // GC support
    bool is_marked() const { return m_marked; }
    void mark() { m_marked = true; }
    void unmark() { m_marked = false; }
    virtual void trace();  // Mark referenced objects

protected:
    std::unordered_map<String, Value> m_properties;
    std::shared_ptr<Object> m_prototype;
    bool m_marked{false};
};

// ============================================================================
// Closure - Function with captured environment
// ============================================================================

struct UpvalueSlot {
    Value* location;
    Value closed;
    bool is_open{true};
};

class Closure : public Object {
public:
    explicit Closure(std::shared_ptr<Function> function);

    [[nodiscard]] Function* function() const { return m_function.get(); }
    [[nodiscard]] bool is_callable() const override { return true; }

    // Upvalues
    void set_upvalue(usize index, std::shared_ptr<UpvalueSlot> upvalue);
    [[nodiscard]] UpvalueSlot* get_upvalue(usize index) const;
    [[nodiscard]] usize upvalue_count() const { return m_upvalues.size(); }

    void trace() override;

private:
    std::shared_ptr<Function> m_function;
    std::vector<std::shared_ptr<UpvalueSlot>> m_upvalues;
};

// ============================================================================
// NativeFunction - Built-in function implemented in C++
// ============================================================================

using NativeFn = std::function<Value(VM& vm, const std::vector<Value>& args)>;

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

// ============================================================================
// Class
// ============================================================================

class Class : public Object {
public:
    explicit Class(String name);

    [[nodiscard]] const String& name() const { return m_name; }
    [[nodiscard]] bool is_class() const override { return true; }
    [[nodiscard]] bool is_callable() const override { return true; }  // Constructor

    // Methods
    void set_method(const String& name, std::shared_ptr<Closure> method);
    [[nodiscard]] Closure* get_method(const String& name) const;
    [[nodiscard]] bool has_method(const String& name) const;

    // Static methods
    void set_static_method(const String& name, std::shared_ptr<Closure> method);
    [[nodiscard]] Closure* get_static_method(const String& name) const;

    // Superclass
    void set_superclass(std::shared_ptr<Class> superclass);
    [[nodiscard]] Class* superclass() const { return m_superclass.get(); }

    void trace() override;

private:
    String m_name;
    std::unordered_map<String, std::shared_ptr<Closure>> m_methods;
    std::unordered_map<String, std::shared_ptr<Closure>> m_static_methods;
    std::shared_ptr<Class> m_superclass;
};

// ============================================================================
// Instance
// ============================================================================

class Instance : public Object {
public:
    explicit Instance(std::shared_ptr<Class> klass);

    [[nodiscard]] Class* klass() const { return m_class.get(); }

    void trace() override;

private:
    std::shared_ptr<Class> m_class;
};

// ============================================================================
// BoundMethod - Method bound to an instance
// ============================================================================

class BoundMethod : public Object {
public:
    BoundMethod(Value receiver, std::shared_ptr<Closure> method);

    [[nodiscard]] const Value& receiver() const { return m_receiver; }
    [[nodiscard]] Closure* method() const { return m_method.get(); }
    [[nodiscard]] bool is_callable() const override { return true; }

    void trace() override;

private:
    Value m_receiver;
    std::shared_ptr<Closure> m_method;
};

} // namespace lithium::js
