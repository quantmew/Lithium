#include <gtest/gtest.h>
#include "lithium/css/parser.hpp"

using namespace lithium;
using namespace lithium::css;

TEST(CSSParserTest, ParsesMediaRuleWithNestedStyle) {
    Parser parser;
    auto sheet = parser.parse_stylesheet(
        "@media screen and (min-width: 900px) { "
        "body { color: red; padding: 1rem; } "
        "}");

    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& variant = sheet.rules[0];
    ASSERT_TRUE(variant.is<AtRule>());

    const auto& media = variant.get<AtRule>();
    EXPECT_EQ(media.name.to_lowercase(), "media");
    ASSERT_TRUE(media.nested_rules.has_value());
    ASSERT_EQ(media.nested_rules->size(), 1u);

    const auto& nested = *media.nested_rules->at(0);
    ASSERT_TRUE(nested.is<StyleRule>());
    const auto& style = nested.get<StyleRule>();

    ASSERT_EQ(style.selectors.selectors.size(), 1u);
    EXPECT_EQ(style.selectors.selectors[0].parts.size(), 1u);
    ASSERT_EQ(style.declarations.declarations.size(), 2u);
    EXPECT_EQ(style.declarations.declarations[0].property, "color");
    EXPECT_EQ(style.declarations.declarations[1].property, "padding");
}

TEST(CSSParserTest, ParsesFontFaceDeclarations) {
    Parser parser;
    auto sheet = parser.parse_stylesheet(
        "@font-face { font-family: 'Test'; src: url(test.woff2); font-weight: 700; }");

    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& variant = sheet.rules[0];
    ASSERT_TRUE(variant.is<AtRule>());

    const auto& font_face = variant.get<AtRule>();
    ASSERT_TRUE(font_face.declarations.has_value());
    const auto& decls = font_face.declarations->declarations;

    ASSERT_EQ(decls.size(), 3u);
    EXPECT_EQ(decls[0].property, "font-family");
    EXPECT_EQ(decls[1].property, "src");
    EXPECT_EQ(decls[2].property, "font-weight");
}
