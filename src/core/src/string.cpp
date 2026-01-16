#include "lithium/core/string.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>

namespace lithium {

// ============================================================================
// UTF-8 implementation
// ============================================================================

namespace unicode {

Utf8DecodeResult utf8_decode(const char* data, usize length) {
    if (length == 0 || data == nullptr) {
        return {INVALID_CODE_POINT, 0};
    }

    auto byte = static_cast<u8>(data[0]);

    // Single byte (ASCII)
    if ((byte & 0x80) == 0) {
        return {static_cast<CodePoint>(byte), 1};
    }

    // Determine sequence length
    usize seq_len;
    CodePoint cp;

    if ((byte & 0xE0) == 0xC0) {
        seq_len = 2;
        cp = byte & 0x1F;
    } else if ((byte & 0xF0) == 0xE0) {
        seq_len = 3;
        cp = byte & 0x0F;
    } else if ((byte & 0xF8) == 0xF0) {
        seq_len = 4;
        cp = byte & 0x07;
    } else {
        // Invalid leading byte
        return {REPLACEMENT_CHARACTER, 1};
    }

    if (length < seq_len) {
        return {REPLACEMENT_CHARACTER, length};
    }

    // Decode continuation bytes
    for (usize i = 1; i < seq_len; ++i) {
        byte = static_cast<u8>(data[i]);
        if ((byte & 0xC0) != 0x80) {
            return {REPLACEMENT_CHARACTER, i};
        }
        cp = (cp << 6) | (byte & 0x3F);
    }

    // Validate code point
    if (!is_valid(cp)) {
        return {REPLACEMENT_CHARACTER, seq_len};
    }

    // Check for overlong encoding
    if (seq_len == 2 && cp < 0x80) {
        return {REPLACEMENT_CHARACTER, seq_len};
    }
    if (seq_len == 3 && cp < 0x800) {
        return {REPLACEMENT_CHARACTER, seq_len};
    }
    if (seq_len == 4 && cp < 0x10000) {
        return {REPLACEMENT_CHARACTER, seq_len};
    }

    return {cp, seq_len};
}

usize utf8_encode(CodePoint cp, char* buffer) {
    if (cp < 0x80) {
        buffer[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        buffer[0] = static_cast<char>(0xC0 | (cp >> 6));
        buffer[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        buffer[0] = static_cast<char>(0xE0 | (cp >> 12));
        buffer[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buffer[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        buffer[0] = static_cast<char>(0xF0 | (cp >> 18));
        buffer[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        buffer[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buffer[3] = static_cast<char>(0x80 | (cp & 0x3F));
        return 4;
    }
    // Invalid code point
    return 0;
}

usize utf8_code_point_length(char first_byte) {
    auto byte = static_cast<u8>(first_byte);
    if ((byte & 0x80) == 0) return 1;
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 1; // Invalid, treat as single byte
}

usize utf8_encoded_length(CodePoint cp) {
    if (cp < 0x80) return 1;
    if (cp < 0x800) return 2;
    if (cp < 0x10000) return 3;
    if (cp <= 0x10FFFF) return 4;
    return 0;
}

} // namespace unicode

// ============================================================================
// String implementation
// ============================================================================

String::String(const char* str) : m_data(str ? str : "") {}

String::String(const char* str, usize length) : m_data(str, length) {}

String::String(std::string str) : m_data(std::move(str)) {}

String::String(std::string_view sv) : m_data(sv) {}

String::String(usize count, char c) : m_data(count, c) {}

String String::from_code_point(unicode::CodePoint cp) {
    char buffer[4];
    usize len = unicode::utf8_encode(cp, buffer);
    return String(buffer, len);
}

String String::from_code_points(const std::vector<unicode::CodePoint>& cps) {
    StringBuilder builder;
    for (auto cp : cps) {
        builder.append(cp);
    }
    return builder.build();
}

usize String::code_point_count() const {
    usize count = 0;
    const char* ptr = m_data.data();
    const char* end = ptr + m_data.size();

    while (ptr < end) {
        usize len = unicode::utf8_code_point_length(*ptr);
        ptr += len;
        ++count;
    }

    return count;
}

// CodePointIterator implementation
String::CodePointIterator::CodePointIterator(const char* ptr, const char* end)
    : m_ptr(ptr), m_end(end) {}

unicode::CodePoint String::CodePointIterator::operator*() const {
    if (m_ptr >= m_end) {
        return unicode::INVALID_CODE_POINT;
    }
    auto result = unicode::utf8_decode(m_ptr, static_cast<usize>(m_end - m_ptr));
    return result.code_point;
}

String::CodePointIterator& String::CodePointIterator::operator++() {
    if (m_ptr < m_end) {
        usize len = unicode::utf8_code_point_length(*m_ptr);
        m_ptr += len;
    }
    return *this;
}

String::CodePointIterator String::CodePointIterator::operator++(int) {
    CodePointIterator tmp = *this;
    ++(*this);
    return tmp;
}

bool String::CodePointIterator::operator==(const CodePointIterator& other) const {
    return m_ptr == other.m_ptr;
}

bool String::CodePointIterator::operator!=(const CodePointIterator& other) const {
    return m_ptr != other.m_ptr;
}

String::CodePointIterator String::code_points_begin() const {
    return CodePointIterator(m_data.data(), m_data.data() + m_data.size());
}

String::CodePointIterator String::code_points_end() const {
    const char* end = m_data.data() + m_data.size();
    return CodePointIterator(end, end);
}

void String::append(const String& other) {
    m_data.append(other.m_data);
}

void String::append(std::string_view sv) {
    m_data.append(sv);
}

void String::append(unicode::CodePoint cp) {
    char buffer[4];
    usize len = unicode::utf8_encode(cp, buffer);
    m_data.append(buffer, len);
}

String String::substring(usize start, usize length) const {
    return String(m_data.substr(start, length));
}

std::optional<usize> String::find(const String& needle, usize start) const {
    auto pos = m_data.find(needle.m_data, start);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    return pos;
}

std::optional<usize> String::find(char c, usize start) const {
    auto pos = m_data.find(c, start);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    return pos;
}

bool String::contains(const String& needle) const {
    return m_data.find(needle.m_data) != std::string::npos;
}

bool String::starts_with(const String& prefix) const {
    return m_data.starts_with(prefix.m_data);
}

bool String::ends_with(const String& suffix) const {
    return m_data.ends_with(suffix.m_data);
}

String String::to_lowercase() const {
    String result;
    result.m_data.reserve(m_data.size());

    for (auto it = code_points_begin(); it != code_points_end(); ++it) {
        result.append(unicode::to_ascii_lower(*it));
    }

    return result;
}

String String::to_uppercase() const {
    String result;
    result.m_data.reserve(m_data.size());

    for (auto it = code_points_begin(); it != code_points_end(); ++it) {
        result.append(unicode::to_ascii_upper(*it));
    }

    return result;
}

String String::trim() const {
    return trim_start().trim_end();
}

String String::trim_start() const {
    auto start = m_data.begin();
    while (start != m_data.end() && unicode::is_ascii_whitespace(static_cast<u8>(*start))) {
        ++start;
    }
    return String(std::string(start, m_data.end()));
}

String String::trim_end() const {
    auto end = m_data.end();
    while (end != m_data.begin() && unicode::is_ascii_whitespace(static_cast<u8>(*(end - 1)))) {
        --end;
    }
    return String(std::string(m_data.begin(), end));
}

std::vector<String> String::split(char delimiter) const {
    std::vector<String> result;
    usize start = 0;
    usize end = m_data.find(delimiter);

    while (end != std::string::npos) {
        result.emplace_back(m_data.substr(start, end - start));
        start = end + 1;
        end = m_data.find(delimiter, start);
    }

    result.emplace_back(m_data.substr(start));
    return result;
}

std::vector<String> String::split(const String& delimiter) const {
    std::vector<String> result;
    usize start = 0;
    usize end = m_data.find(delimiter.m_data);

    while (end != std::string::npos) {
        result.emplace_back(m_data.substr(start, end - start));
        start = end + delimiter.size();
        end = m_data.find(delimiter.m_data, start);
    }

    result.emplace_back(m_data.substr(start));
    return result;
}

bool String::equals_ignore_case(const String& other) const {
    return to_lowercase() == other.to_lowercase();
}

String String::operator+(const String& other) const {
    return String(m_data + other.m_data);
}

String& String::operator+=(const String& other) {
    m_data += other.m_data;
    return *this;
}

String& String::operator+=(std::string_view sv) {
    m_data += sv;
    return *this;
}

String& String::operator+=(unicode::CodePoint cp) {
    append(cp);
    return *this;
}

// ============================================================================
// StringBuilder implementation
// ============================================================================

StringBuilder::StringBuilder(usize initial_capacity) {
    m_buffer.reserve(initial_capacity);
}

StringBuilder& StringBuilder::append(const String& str) {
    m_buffer.append(str.std_string());
    return *this;
}

StringBuilder& StringBuilder::append(std::string_view sv) {
    m_buffer.append(sv);
    return *this;
}

StringBuilder& StringBuilder::append(const char* str) {
    if (str) {
        m_buffer.append(str);
    }
    return *this;
}

StringBuilder& StringBuilder::append(char c) {
    m_buffer.push_back(c);
    return *this;
}

StringBuilder& StringBuilder::append(unicode::CodePoint cp) {
    char buffer[4];
    usize len = unicode::utf8_encode(cp, buffer);
    m_buffer.append(buffer, len);
    return *this;
}

StringBuilder& StringBuilder::append(i64 value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    m_buffer.append(buffer, static_cast<usize>(result.ptr - buffer));
    return *this;
}

StringBuilder& StringBuilder::append(u64 value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    m_buffer.append(buffer, static_cast<usize>(result.ptr - buffer));
    return *this;
}

StringBuilder& StringBuilder::append(f64 value) {
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    m_buffer.append(buffer, static_cast<usize>(result.ptr - buffer));
    return *this;
}

} // namespace lithium
