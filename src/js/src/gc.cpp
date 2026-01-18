/**
 * Garbage Collector implementation - Mark and Sweep
 */

#include "lithium/js/gc.hpp"
#include "lithium/js/object.hpp"
#include "lithium/js/value.hpp"
#include "lithium/js/vm.hpp"
#include <algorithm>

#ifdef LITHIUM_DEBUG_GC
#include <iostream>
#endif

namespace lithium::js {

// ============================================================================
// GarbageCollector implementation
// ============================================================================

GarbageCollector::GarbageCollector() = default;

GarbageCollector::~GarbageCollector() = default;

void GarbageCollector::track(std::shared_ptr<Object> obj) {
    m_objects.push_back(std::move(obj));
    m_bytes_allocated += sizeof(Object); // Approximate
}

void GarbageCollector::collect(VM& vm) {
#ifdef LITHIUM_DEBUG_GC
    if (m_log_gc) {
        std::cout << "-- GC begin\n";
        std::cout << "   " << m_objects.size() << " objects tracked\n";
    }
#endif

    usize before = m_objects.size();

    // Mark phase
    mark_roots(vm);
    trace_references();

    // Sweep phase
    sweep();

    // Update threshold
    m_next_gc = m_bytes_allocated * static_cast<usize>(m_heap_grow_factor);
    ++m_gc_count;

#ifdef LITHIUM_DEBUG_GC
    if (m_log_gc) {
        std::cout << "   collected " << (before - m_objects.size()) << " objects\n";
        std::cout << "   " << m_objects.size() << " remaining\n";
        std::cout << "-- GC end\n";
    }
#endif
}

void GarbageCollector::push_root(const Value* value) {
    m_roots.push_back(value);
}

void GarbageCollector::pop_root() {
    if (!m_roots.empty()) {
        m_roots.pop_back();
    }
}

void GarbageCollector::mark_roots(VM& vm) {
    // Mark VM roots (stack, globals, call frames)
    vm.mark_roots(*this);

    // Mark temporary roots
    for (const Value* root : m_roots) {
        if (root) {
            mark_value(*root);
        }
    }
}

void GarbageCollector::mark_value(const Value& value) {
    if (value.is_object()) {
        mark_object(value.as_object());
    }
}

void GarbageCollector::mark_object(Object* obj) {
    if (!obj || obj->is_marked()) {
        return;
    }

    obj->mark();
    m_gray_stack.push_back(obj);

#ifdef LITHIUM_DEBUG_GC
    if (m_log_gc) {
        std::cout << "   mark " << static_cast<void*>(obj) << "\n";
    }
#endif
}

void GarbageCollector::trace_references() {
    while (!m_gray_stack.empty()) {
        Object* obj = m_gray_stack.back();
        m_gray_stack.pop_back();
        // Pass *this (GC instance) to trace() so it can properly mark nested objects
        obj->trace(*this);
    }
}

void GarbageCollector::sweep() {
    // Remove unmarked objects
    auto it = std::remove_if(m_objects.begin(), m_objects.end(),
        [this](const std::shared_ptr<Object>& obj) {
            if (!obj->is_marked()) {
#ifdef LITHIUM_DEBUG_GC
                if (m_log_gc) {
                    std::cout << "   sweep " << static_cast<void*>(obj.get()) << "\n";
                }
#endif
                m_bytes_allocated -= sizeof(Object);
                return true;  // Remove
            }
            obj->unmark();
            return false;  // Keep
        });

    m_objects.erase(it, m_objects.end());
}

} // namespace lithium::js
