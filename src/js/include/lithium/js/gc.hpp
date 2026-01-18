#pragma once

#include "lithium/core/types.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace lithium::js {

class Object;
class Value;
class VM;

// ============================================================================
// Garbage Collector - Mark and Sweep
// ============================================================================

class GarbageCollector {
    friend class VM;  // VM needs access to mark_roots()

public:
    GarbageCollector();
    ~GarbageCollector();

    // Configuration
    void set_heap_grow_factor(f64 factor) { m_heap_grow_factor = factor; }
    void set_initial_threshold(usize bytes) { m_next_gc = bytes; }

    // GC stats
    [[nodiscard]] usize bytes_allocated() const { return m_bytes_allocated; }
    [[nodiscard]] usize total_objects() const { return m_objects.size(); }
    [[nodiscard]] usize gc_count() const { return m_gc_count; }

    // Object tracking
    template<typename T, typename... Args>
    std::shared_ptr<T> allocate(Args&&... args) {
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        track(obj);
        return obj;
    }

    void track(std::shared_ptr<Object> obj);

    // Manual GC trigger
    void collect(VM& vm);

    // Check if GC should run
    [[nodiscard]] bool should_collect() const {
        return m_stress_gc || m_bytes_allocated > m_next_gc;
    }

    // Stress testing (GC on every allocation)
    void set_stress_gc(bool enabled) { m_stress_gc = enabled; }

    // Debug
    void set_log_gc(bool enabled) { m_log_gc = enabled; }

    // Root registration (for temporary roots)
    void push_root(const Value* value);
    void pop_root();

    // Mark phase - public so Object::trace() can use them
    void mark_value(const Value& value);
    void mark_object(Object* obj);

private:
    // Mark phase - internal methods
    void mark_roots(VM& vm);
    void trace_references();

    // Sweep phase
    void sweep();

    // All managed objects
    std::vector<std::shared_ptr<Object>> m_objects;

    // Gray stack for mark phase
    std::vector<Object*> m_gray_stack;

    // Temporary roots
    std::vector<const Value*> m_roots;

    // Memory tracking
    usize m_bytes_allocated{0};
    usize m_next_gc{1024 * 1024};  // 1MB initial threshold
    f64 m_heap_grow_factor{2.0};

    // Stats
    usize m_gc_count{0};

    // Debug flags
    bool m_stress_gc{false};
    bool m_log_gc{false};
};

// ============================================================================
// RAII helpers
// ============================================================================

class GCRootGuard {
public:
    GCRootGuard(GarbageCollector& gc, const Value* value)
        : m_gc(gc) {
        m_gc.push_root(value);
    }

    ~GCRootGuard() {
        m_gc.pop_root();
    }

    GCRootGuard(const GCRootGuard&) = delete;
    GCRootGuard& operator=(const GCRootGuard&) = delete;

private:
    GarbageCollector& m_gc;
};

#define GC_ROOT(gc, value) \
    ::lithium::js::GCRootGuard _gc_root_##__LINE__(gc, &(value))

} // namespace lithium::js
