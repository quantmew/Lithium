#pragma once

#include "types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace lithium {

// ============================================================================
// Unicode utilities
// ============================================================================

namespace unicode {

// Unicode code point type
using CodePoint = char32_t;

// Invalid code point marker
constexpr CodePoint REPLACEMENT_CHARACTER = 0xFFFD;
constexpr CodePoint INVALID_CODE_POINT = 0xFFFFFFFF;

// Check if code point is valid
[[nodiscard]] constexpr bool is_valid(CodePoint cp) {
    return cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
}

// Check character categories
[[nodiscard]] constexpr bool is_ascii(CodePoint cp) {
    return cp <= 0x7F;
}

[[nodiscard]] constexpr bool is_ascii_alpha(CodePoint cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

[[nodiscard]] constexpr bool is_ascii_digit(CodePoint cp) {
    return cp >= '0' && cp <= '9';
}

[[nodiscard]] constexpr bool is_ascii_alphanumeric(CodePoint cp) {
    return is_ascii_alpha(cp) || is_ascii_digit(cp);
}

[[nodiscard]] constexpr bool is_ascii_whitespace(CodePoint cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f';
}

[[nodiscard]] constexpr bool is_ascii_hex_digit(CodePoint cp) {
    return is_ascii_digit(cp) ||
           (cp >= 'A' && cp <= 'F') ||
           (cp >= 'a' && cp <= 'f');
}

[[nodiscard]] constexpr bool is_ascii_upper(CodePoint cp) {
    return cp >= 'A' && cp <= 'Z';
}

[[nodiscard]] constexpr bool is_ascii_lower(CodePoint cp) {
    return cp >= 'a' && cp <= 'z';
}

[[nodiscard]] constexpr CodePoint to_ascii_lower(CodePoint cp) {
    if (is_ascii_upper(cp)) {
        return cp + ('a' - 'A');
    }
    return cp;
}

[[nodiscard]] constexpr CodePoint to_ascii_upper(CodePoint cp) {
    if (is_ascii_lower(cp)) {
        return cp - ('a' - 'A');
    }
    return cp;
}

// UTF-8 encoding/decoding
struct Utf8DecodeResult {
    CodePoint code_point;
    usize bytes_consumed;
};

[[nodiscard]] Utf8DecodeResult utf8_decode(const char* data, usize length);
[[nodiscard]] usize utf8_encode(CodePoint cp, char* buffer);
[[nodiscard]] usize utf8_code_point_length(char first_byte);
[[nodiscard]] usize utf8_encoded_length(CodePoint cp);

} // namespace unicode

// ============================================================================
// String - UTF-8 encoded string with utilities
// ============================================================================

class String {
public:
    using iterator = std::string::iterator;
    using const_iterator = std::string::const_iterator;

    // Constructors
    String() = default;
    String(const char* str);
    String(const char* str, usize length);
    String(std::string str);
    String(std::string_view sv);
    String(usize count, char c);

    // From code points
    static String from_code_point(unicode::CodePoint cp);
    static String from_code_points(const std::vector<unicode::CodePoint>& cps);

    // Access
    [[nodiscard]] const char* c_str() const noexcept { return m_data.c_str(); }
    [[nodiscard]] const char* data() const noexcept { return m_data.data(); }
    [[nodiscard]] usize size() const noexcept { return m_data.size(); }
    [[nodiscard]] usize length() const noexcept { return m_data.length(); }
    [[nodiscard]] bool empty() const noexcept { return m_data.empty(); }

    [[nodiscard]] std::string_view view() const noexcept {
        return std::string_view(m_data);
    }

    [[nodiscard]] const std::string& std_string() const noexcept {
        return m_data;
    }

    // Unicode-aware length
    [[nodiscard]] usize code_point_count() const;

    // Iterators
    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cbegin() const { return m_data.cbegin(); }
    const_iterator cend() const { return m_data.cend(); }

    // Code point iterator
    class CodePointIterator {
    public:
        CodePointIterator(const char* ptr, const char* end);

        [[nodiscard]] unicode::CodePoint operator*() const;
        CodePointIterator& operator++();
        CodePointIterator operator++(int);

        [[nodiscard]] bool operator==(const CodePointIterator& other) const;
        [[nodiscard]] bool operator!=(const CodePointIterator& other) const;

    private:
        const char* m_ptr;
        const char* m_end;
    };

    [[nodiscard]] CodePointIterator code_points_begin() const;
    [[nodiscard]] CodePointIterator code_points_end() const;

    // Modification
    void append(const String& other);
    void append(std::string_view sv);
    void append(unicode::CodePoint cp);
    void clear() { m_data.clear(); }

    // Substring
    [[nodiscard]] String substring(usize start, usize length = std::string::npos) const;

    // Search
    [[nodiscard]] std::optional<usize> find(const String& needle, usize start = 0) const;
    [[nodiscard]] std::optional<usize> find(char c, usize start = 0) const;
    [[nodiscard]] bool contains(const String& needle) const;
    [[nodiscard]] bool starts_with(const String& prefix) const;
    [[nodiscard]] bool ends_with(const String& suffix) const;

    // Transformations
    [[nodiscard]] String to_lowercase() const;
    [[nodiscard]] String to_uppercase() const;
    [[nodiscard]] String trim() const;
    [[nodiscard]] String trim_start() const;
    [[nodiscard]] String trim_end() const;

    // Split
    [[nodiscard]] std::vector<String> split(char delimiter) const;
    [[nodiscard]] std::vector<String> split(const String& delimiter) const;

    // Comparison
    [[nodiscard]] bool equals_ignore_case(const String& other) const;

    [[nodiscard]] bool operator==(const String& other) const {
        return m_data == other.m_data;
    }

    [[nodiscard]] bool operator!=(const String& other) const {
        return m_data != other.m_data;
    }

    [[nodiscard]] bool operator<(const String& other) const {
        return m_data < other.m_data;
    }

    // Concatenation
    String operator+(const String& other) const;
    String& operator+=(const String& other);
    String& operator+=(std::string_view sv);
    String& operator+=(unicode::CodePoint cp);

    // Index access (byte-level, use with caution)
    char& operator[](usize index) { return m_data[index]; }
    const char& operator[](usize index) const { return m_data[index]; }

private:
    std::string m_data;
};

// String literal operator
inline String operator""_s(const char* str, std::size_t len) {
    return String(str, len);
}

// ============================================================================
// StringBuilder - Efficient string building
// ============================================================================

class StringBuilder {
public:
    StringBuilder() = default;
    explicit StringBuilder(usize initial_capacity);

    StringBuilder& append(const String& str);
    StringBuilder& append(std::string_view sv);
    StringBuilder& append(const char* str);
    StringBuilder& append(char c);
    StringBuilder& append(unicode::CodePoint cp);
    StringBuilder& append(i64 value);
    StringBuilder& append(u64 value);
    StringBuilder& append(f64 value);

    template<typename... Args>
    StringBuilder& append_format(std::string_view format, Args&&... args);

    void clear() { m_buffer.clear(); }
    void reserve(usize capacity) { m_buffer.reserve(capacity); }

    [[nodiscard]] String build() const { return String(m_buffer); }
    [[nodiscard]] std::string_view view() const { return m_buffer; }
    [[nodiscard]] usize size() const { return m_buffer.size(); }
    [[nodiscard]] bool empty() const { return m_buffer.empty(); }

private:
    std::string m_buffer;
};

} // namespace lithium
