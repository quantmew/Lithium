/**
 * String Interning Pool implementation
 */

#include "lithium/js/string_intern.hpp"

namespace lithium::js {

StringInternPool::StringInternPool() {
    // Pre-intern common JavaScript strings
    intern_common_strings();
}

StringInternPool::~StringInternPool() {
    // Clean up all interned strings
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto* s : m_pool) {
        delete s;
    }
    m_pool.clear();
}

StringInternPool& StringInternPool::instance() {
    static StringInternPool pool;
    return pool;
}

InternedString* StringInternPool::intern(const String& str) {
    // Fast path for empty strings
    if (str.empty()) {
        static InternedString empty_string(""_s);
        return &empty_string;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a temporary for lookup
    InternedString lookup_key(str);

    // Check if already interned
    auto it = m_pool.find(&lookup_key);
    if (it != m_pool.end()) {
        // Found - increment ref count and return
        (*it)->ref_count.fetch_add(1, std::memory_order_relaxed);
        return *it;
    }

    // Not found - create new interned string
    auto* interned = new InternedString(str);
    m_pool.insert(interned);
    return interned;
}

void StringInternPool::inc_ref(InternedString* s) {
    if (s) {
        s->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void StringInternPool::dec_ref(InternedString* s) {
    if (!s) return;

    // Decrement ref count
    if (s->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Ref count reached 0 - can remove from pool
        // For now, we keep strings in the pool to avoid the cost of re-interning
        // This is a common optimization - interned strings are rarely truly "dead"

        // If we wanted to remove dead strings:
        // std::lock_guard<std::mutex> lock(m_mutex);
        // m_pool.erase(s);
        // delete s;
    }
}

size_t StringInternPool::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

void StringInternPool::intern_common_strings() {
    // Pre-intern common JavaScript property names and keywords
    // These are very frequently used and benefit from early interning

    // Common property names
    intern("length"_s);
    intern("prototype"_s);
    intern("constructor"_s);
    intern("__proto__"_s);
    intern("toString"_s);
    intern("valueOf"_s);
    intern("hasOwnProperty"_s);

    // Common variable names
    intern("undefined"_s);
    intern("null"_s);
    intern("true"_s);
    intern("false"_s);
    intern("NaN"_s);
    intern("Infinity"_s);

    // Common object property names (for nbody benchmark etc.)
    intern("x"_s);
    intern("y"_s);
    intern("z"_s);
    intern("vx"_s);
    intern("vy"_s);
    intern("vz"_s);
    intern("mass"_s);

    // Array methods
    intern("push"_s);
    intern("pop"_s);
    intern("shift"_s);
    intern("unshift"_s);
    intern("slice"_s);
    intern("splice"_s);
    intern("concat"_s);
    intern("join"_s);
    intern("indexOf"_s);
    intern("forEach"_s);
    intern("map"_s);
    intern("filter"_s);
    intern("reduce"_s);

    // Math properties
    intern("PI"_s);
    intern("E"_s);
    intern("sqrt"_s);
    intern("abs"_s);
    intern("floor"_s);
    intern("ceil"_s);
    intern("round"_s);
    intern("sin"_s);
    intern("cos"_s);
    intern("tan"_s);
    intern("log"_s);
    intern("exp"_s);
    intern("pow"_s);
    intern("min"_s);
    intern("max"_s);
    intern("random"_s);

    // Console
    intern("console"_s);
    intern("log"_s);
    intern("error"_s);
    intern("warn"_s);

    // Function properties
    intern("call"_s);
    intern("apply"_s);
    intern("bind"_s);
    intern("name"_s);
    intern("arguments"_s);
    intern("caller"_s);

    // Object methods
    intern("keys"_s);
    intern("values"_s);
    intern("entries"_s);
    intern("assign"_s);
    intern("create"_s);
    intern("freeze"_s);
    intern("seal"_s);

    // String methods
    intern("charAt"_s);
    intern("charCodeAt"_s);
    intern("substring"_s);
    intern("substr"_s);
    intern("split"_s);
    intern("trim"_s);
    intern("toLowerCase"_s);
    intern("toUpperCase"_s);
    intern("replace"_s);
    intern("match"_s);
    intern("search"_s);

    // Number/Date
    intern("toFixed"_s);
    intern("toPrecision"_s);
    intern("getTime"_s);
    intern("getFullYear"_s);
    intern("getMonth"_s);
    intern("getDate"_s);
    intern("getHours"_s);
    intern("getMinutes"_s);
    intern("getSeconds"_s);
}

} // namespace lithium::js
