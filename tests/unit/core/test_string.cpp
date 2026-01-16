#include <gtest/gtest.h>
#include "lithium/core/string.hpp"

using namespace lithium;

// ============================================================================
// Unicode Tests
// ============================================================================

TEST(UnicodeTest, IsAscii) {
    EXPECT_TRUE(unicode::is_ascii('A'));
    EXPECT_TRUE(unicode::is_ascii(0));
    EXPECT_TRUE(unicode::is_ascii(127));
    EXPECT_FALSE(unicode::is_ascii(128));
    EXPECT_FALSE(unicode::is_ascii(0x4E2D));  // Chinese character
}

TEST(UnicodeTest, AsciiCaseConversion) {
    EXPECT_EQ(unicode::to_ascii_lower('A'), 'a');
    EXPECT_EQ(unicode::to_ascii_lower('Z'), 'z');
    EXPECT_EQ(unicode::to_ascii_lower('a'), 'a');
    EXPECT_EQ(unicode::to_ascii_lower('1'), '1');

    EXPECT_EQ(unicode::to_ascii_upper('a'), 'A');
    EXPECT_EQ(unicode::to_ascii_upper('z'), 'Z');
    EXPECT_EQ(unicode::to_ascii_upper('A'), 'A');
    EXPECT_EQ(unicode::to_ascii_upper('1'), '1');
}

TEST(UnicodeTest, Utf8DecodeAscii) {
    const char* text = "Hello";
    auto result = unicode::utf8_decode(text, 5);

    EXPECT_EQ(result.code_point, 'H');
    EXPECT_EQ(result.bytes_consumed, 1u);
}

TEST(UnicodeTest, Utf8DecodeTwoBytes) {
    const char* text = "\xC3\xA9";  // é (U+00E9)
    auto result = unicode::utf8_decode(text, 2);

    EXPECT_EQ(result.code_point, 0x00E9u);
    EXPECT_EQ(result.bytes_consumed, 2u);
}

TEST(UnicodeTest, Utf8DecodeThreeBytes) {
    const char* text = "\xE4\xB8\xAD";  // 中 (U+4E2D)
    auto result = unicode::utf8_decode(text, 3);

    EXPECT_EQ(result.code_point, 0x4E2Du);
    EXPECT_EQ(result.bytes_consumed, 3u);
}

TEST(UnicodeTest, Utf8DecodeFourBytes) {
    const char* text = "\xF0\x9F\x98\x80";  // 😀 (U+1F600)
    auto result = unicode::utf8_decode(text, 4);

    EXPECT_EQ(result.code_point, 0x1F600u);
    EXPECT_EQ(result.bytes_consumed, 4u);
}

TEST(UnicodeTest, Utf8Encode) {
    char buffer[4];

    // ASCII
    EXPECT_EQ(unicode::utf8_encode('A', buffer), 1u);
    EXPECT_EQ(buffer[0], 'A');

    // Two bytes
    EXPECT_EQ(unicode::utf8_encode(0x00E9, buffer), 2u);
    EXPECT_EQ(static_cast<u8>(buffer[0]), 0xC3);
    EXPECT_EQ(static_cast<u8>(buffer[1]), 0xA9);

    // Three bytes
    EXPECT_EQ(unicode::utf8_encode(0x4E2D, buffer), 3u);
    EXPECT_EQ(static_cast<u8>(buffer[0]), 0xE4);
    EXPECT_EQ(static_cast<u8>(buffer[1]), 0xB8);
    EXPECT_EQ(static_cast<u8>(buffer[2]), 0xAD);

    // Four bytes
    EXPECT_EQ(unicode::utf8_encode(0x1F600, buffer), 4u);
    EXPECT_EQ(static_cast<u8>(buffer[0]), 0xF0);
    EXPECT_EQ(static_cast<u8>(buffer[1]), 0x9F);
    EXPECT_EQ(static_cast<u8>(buffer[2]), 0x98);
    EXPECT_EQ(static_cast<u8>(buffer[3]), 0x80);
}

// ============================================================================
// String Tests
// ============================================================================

TEST(StringTest, DefaultConstruction) {
    String s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(StringTest, CStringConstruction) {
    String s("Hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_FALSE(s.empty());
}

TEST(StringTest, CodePointCount) {
    String ascii("Hello");
    EXPECT_EQ(ascii.code_point_count(), 5u);

    String unicode("Hello\xE4\xB8\xAD");  // Hello中
    EXPECT_EQ(unicode.code_point_count(), 6u);

    String emoji("\xF0\x9F\x98\x80\xF0\x9F\x98\x81");  // 😀😁
    EXPECT_EQ(emoji.code_point_count(), 2u);
}

TEST(StringTest, CodePointIteration) {
    String s("A\xC3\xA9\xE4\xB8\xAD");  // Aé中

    std::vector<unicode::CodePoint> code_points;
    for (auto it = s.code_points_begin(); it != s.code_points_end(); ++it) {
        code_points.push_back(*it);
    }

    ASSERT_EQ(code_points.size(), 3u);
    EXPECT_EQ(code_points[0], 'A');
    EXPECT_EQ(code_points[1], 0x00E9u);
    EXPECT_EQ(code_points[2], 0x4E2Du);
}

TEST(StringTest, Concatenation) {
    String a("Hello");
    String b(" World");
    String c = a + b;

    EXPECT_EQ(c, String("Hello World"));
}

TEST(StringTest, Contains) {
    String s("Hello World");

    EXPECT_TRUE(s.contains(String("World")));
    EXPECT_TRUE(s.contains(String("Hello")));
    EXPECT_FALSE(s.contains(String("Foo")));
}

TEST(StringTest, StartsWith) {
    String s("Hello World");

    EXPECT_TRUE(s.starts_with(String("Hello")));
    EXPECT_TRUE(s.starts_with(String("H")));
    EXPECT_FALSE(s.starts_with(String("World")));
}

TEST(StringTest, EndsWith) {
    String s("Hello World");

    EXPECT_TRUE(s.ends_with(String("World")));
    EXPECT_TRUE(s.ends_with(String("d")));
    EXPECT_FALSE(s.ends_with(String("Hello")));
}

TEST(StringTest, ToLowercase) {
    String s("Hello WORLD");
    EXPECT_EQ(s.to_lowercase(), String("hello world"));
}

TEST(StringTest, ToUppercase) {
    String s("Hello world");
    EXPECT_EQ(s.to_uppercase(), String("HELLO WORLD"));
}

TEST(StringTest, Trim) {
    String s("  Hello World  ");
    EXPECT_EQ(s.trim(), String("Hello World"));
    EXPECT_EQ(s.trim_start(), String("Hello World  "));
    EXPECT_EQ(s.trim_end(), String("  Hello World"));
}

TEST(StringTest, Split) {
    String s("a,b,c,d");
    auto parts = s.split(',');

    ASSERT_EQ(parts.size(), 4u);
    EXPECT_EQ(parts[0], String("a"));
    EXPECT_EQ(parts[1], String("b"));
    EXPECT_EQ(parts[2], String("c"));
    EXPECT_EQ(parts[3], String("d"));
}

TEST(StringTest, Find) {
    String s("Hello World");

    EXPECT_EQ(s.find(String("World")), 6u);
    EXPECT_EQ(s.find('o'), 4u);
    EXPECT_EQ(s.find(String("Foo")), std::nullopt);
}

TEST(StringTest, Substring) {
    String s("Hello World");

    EXPECT_EQ(s.substring(0, 5), String("Hello"));
    EXPECT_EQ(s.substring(6), String("World"));
}

TEST(StringTest, EqualsIgnoreCase) {
    String a("Hello");
    String b("HELLO");
    String c("hello");
    String d("World");

    EXPECT_TRUE(a.equals_ignore_case(b));
    EXPECT_TRUE(a.equals_ignore_case(c));
    EXPECT_FALSE(a.equals_ignore_case(d));
}

// ============================================================================
// StringBuilder Tests
// ============================================================================

TEST(StringBuilderTest, BasicUsage) {
    StringBuilder sb;
    sb.append("Hello");
    sb.append(' ');
    sb.append(String("World"));

    EXPECT_EQ(sb.build(), String("Hello World"));
}

TEST(StringBuilderTest, AppendNumber) {
    StringBuilder sb;
    sb.append(i64{42});
    sb.append(" ");
    sb.append(u64{100});

    EXPECT_EQ(sb.build(), String("42 100"));
}

TEST(StringBuilderTest, AppendCodePoint) {
    StringBuilder sb;
    sb.append(unicode::CodePoint('A'));
    sb.append(unicode::CodePoint(0x4E2D));  // 中

    String result = sb.build();
    EXPECT_EQ(result.code_point_count(), 2u);
}
