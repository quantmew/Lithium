/**
 * String Interning Pool for JavaScript engine
 *
 * String interning ensures that identical strings share the same memory address.
 * This enables O(1) string comparison via pointer equality instead of O(n)
 * character-by-character comparison.
 *
 * Key benefits:
 * - Fast property name comparison in object lookup
 * - Reduced memory usage for duplicate strings
 * - Enables pointer-based hashing for property maps
 */

#pragma once

#include "lithium/core/string.hpp"
#include "lithium/core/types.hpp"
#include <atomic>
#include <mutex>
#include <unordered_set>

namespace lithium::js {

// Reference-counted interned string
struct InternedString {
    std::atomic<u32> ref_count{1};
    String str;

    explicit InternedString(const String& s) : str(s) {}
    explicit InternedString(String&& s) : str(std::move(s)) {}
};

// Hash functor for InternedString pointers (hash by string content)
struct InternedStringHash {
    size_t operator()(const InternedString* s) const {
        return std::hash<std::string>{}(s->str.std_string());
    }
};

// Equality functor for InternedString pointers (compare by string content)
struct InternedStringEqual {
    bool operator()(const InternedString* a, const InternedString* b) const {
        return a->str == b->str;
    }
};

/**
 * Global string intern pool.
 *
 * Thread-safe singleton that manages all interned strings.
 * Strings are stored with reference counting - when ref_count drops to 0,
 * the string can be removed from the pool (but we keep common strings).
 */
class StringInternPool {
public:
    static StringInternPool& instance();

    // Get or create an interned string
    // Returns a pointer that remains valid as long as ref_count > 0
    InternedString* intern(const String& str);

    // Increment reference count
    static void inc_ref(InternedString* s);

    // Decrement reference count (may delete the string)
    void dec_ref(InternedString* s);

    // Statistics
    [[nodiscard]] size_t size() const;

    // Pre-intern common strings (call during VM initialization)
    void intern_common_strings();

private:
    StringInternPool();
    ~StringInternPool();

    // Non-copyable
    StringInternPool(const StringInternPool&) = delete;
    StringInternPool& operator=(const StringInternPool&) = delete;

    // The intern pool - stores all unique strings
    std::unordered_set<InternedString*, InternedStringHash, InternedStringEqual> m_pool;

    // Mutex for thread safety
    mutable std::mutex m_mutex;
};

// Convenience function for interning
inline InternedString* intern_string(const String& s) {
    return StringInternPool::instance().intern(s);
}

} // namespace lithium::js
