#pragma once

#include "types.hpp"
#include <memory>
#include <vector>
#include <cstring>

namespace lithium {

// ============================================================================
// Memory utilities
// ============================================================================

namespace memory {

// Aligned allocation
[[nodiscard]] void* aligned_alloc(usize size, usize alignment);
void aligned_free(void* ptr);

// Memory span - non-owning view over contiguous memory
template<typename T>
class Span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = usize;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;

    constexpr Span() noexcept : m_data(nullptr), m_size(0) {}

    constexpr Span(pointer data, size_type size) noexcept
        : m_data(data), m_size(size) {}

    template<usize N>
    constexpr Span(T (&arr)[N]) noexcept
        : m_data(arr), m_size(N) {}

    template<typename Container>
    constexpr Span(Container& container)
        : m_data(container.data()), m_size(container.size()) {}

    [[nodiscard]] constexpr pointer data() const noexcept { return m_data; }
    [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept {
        return m_size * sizeof(T);
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    [[nodiscard]] constexpr reference operator[](size_type index) const {
        return m_data[index];
    }

    [[nodiscard]] constexpr reference front() const { return m_data[0]; }
    [[nodiscard]] constexpr reference back() const { return m_data[m_size - 1]; }

    [[nodiscard]] constexpr iterator begin() const noexcept { return m_data; }
    [[nodiscard]] constexpr iterator end() const noexcept { return m_data + m_size; }

    [[nodiscard]] constexpr Span subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const {
        if (count == static_cast<size_type>(-1)) {
            count = m_size - offset;
        }
        return Span(m_data + offset, count);
    }

    [[nodiscard]] constexpr Span first(size_type count) const {
        return Span(m_data, count);
    }

    [[nodiscard]] constexpr Span last(size_type count) const {
        return Span(m_data + m_size - count, count);
    }

private:
    pointer m_data;
    size_type m_size;
};

// Byte span aliases
using ByteSpan = Span<u8>;
using ByteSpanConst = Span<const u8>;

// ============================================================================
// Arena allocator - Fast bump allocator for temporary allocations
// ============================================================================

class Arena {
public:
    explicit Arena(usize initial_size = 4096);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& other) noexcept;
    Arena& operator=(Arena&& other) noexcept;

    // Allocate memory (returns nullptr if out of memory)
    [[nodiscard]] void* allocate(usize size, usize alignment = alignof(std::max_align_t));

    // Typed allocation
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        if (!ptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // Allocate array
    template<typename T>
    [[nodiscard]] T* allocate_array(usize count) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    // Reset arena (invalidates all allocations)
    void reset();

    // Get memory usage statistics
    [[nodiscard]] usize used() const { return m_used; }
    [[nodiscard]] usize capacity() const { return m_capacity; }

private:
    void grow(usize min_size);

    u8* m_data{nullptr};
    usize m_capacity{0};
    usize m_used{0};
    std::vector<u8*> m_blocks;
};

// ============================================================================
// Pool allocator - Fixed-size object pool
// ============================================================================

template<typename T, usize BlockSize = 64>
class Pool {
public:
    Pool() = default;
    ~Pool() {
        clear();
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    template<typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* ptr = allocate();
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    void clear() {
        for (auto* block : m_blocks) {
            ::operator delete(block);
        }
        m_blocks.clear();
        m_free_list = nullptr;
    }

private:
    struct FreeNode {
        FreeNode* next;
    };

    [[nodiscard]] void* allocate() {
        if (!m_free_list) {
            allocate_block();
        }
        FreeNode* node = m_free_list;
        m_free_list = node->next;
        return node;
    }

    void deallocate(void* ptr) {
        auto* node = static_cast<FreeNode*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
    }

    void allocate_block() {
        static_assert(sizeof(T) >= sizeof(FreeNode), "T must be at least pointer-sized");
        constexpr usize obj_size = sizeof(T) > sizeof(FreeNode) ? sizeof(T) : sizeof(FreeNode);
        constexpr usize alignment = alignof(T) > alignof(FreeNode) ? alignof(T) : alignof(FreeNode);

        usize block_size = obj_size * BlockSize;
        auto* block = static_cast<u8*>(::operator new(block_size, std::align_val_t{alignment}));
        m_blocks.push_back(block);

        // Build free list
        for (usize i = 0; i < BlockSize; ++i) {
            auto* node = reinterpret_cast<FreeNode*>(block + i * obj_size);
            node->next = m_free_list;
            m_free_list = node;
        }
    }

    std::vector<u8*> m_blocks;
    FreeNode* m_free_list{nullptr};
};

} // namespace memory

// Convenient aliases
using memory::Span;
using memory::ByteSpan;
using memory::Arena;

} // namespace lithium
