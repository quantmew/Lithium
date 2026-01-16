#include <gtest/gtest.h>
#include "lithium/html/parser.hpp"

using namespace lithium;
using namespace lithium::html;

// ============================================================================
// HTML Parser Tests
// ============================================================================

class HTMLParserTest : public ::testing::Test {
protected:
    RefPtr<dom::Document> parse(const String& html) {
        Parser parser;
        return parser.parse(html);
    }
};

TEST_F(HTMLParserTest, EmptyDocument) {
    auto doc = parse("");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->document_element(), nullptr);
}

TEST_F(HTMLParserTest, SimpleDocument) {
    auto doc = parse("<html><head></head><body></body></html>");

    ASSERT_NE(doc, nullptr);
    auto* html = doc->document_element();
    ASSERT_NE(html, nullptr);
    EXPECT_EQ(html->tag_name(), String("html"));
}

TEST_F(HTMLParserTest, ImplicitElements) {
    // HTML parser should create implicit elements
    auto doc = parse("<p>Hello</p>");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->document_element(), nullptr);
    EXPECT_NE(doc->head(), nullptr);
    EXPECT_NE(doc->body(), nullptr);
}

TEST_F(HTMLParserTest, TextContent) {
    auto doc = parse("<p>Hello World</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("Hello World"));
}

TEST_F(HTMLParserTest, NestedElements) {
    auto doc = parse("<div><p><span>Text</span></p></div>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name(), String("div"));

    auto* p = div->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));

    auto* span = p->first_element_child();
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name(), String("span"));
}

TEST_F(HTMLParserTest, Attributes) {
    auto doc = parse("<div id=\"test\" class=\"foo bar\"></div>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(div->id(), String("test"));
    EXPECT_EQ(div->class_name(), String("foo bar"));
}

TEST_F(HTMLParserTest, VoidElements) {
    auto doc = parse("<p>Line 1<br>Line 2</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);

    // Should have: text, br, text
    EXPECT_EQ(p->child_nodes().size(), 3u);
}

TEST_F(HTMLParserTest, Doctype) {
    auto doc = parse("<!DOCTYPE html><html></html>");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->doctype(), nullptr);
    EXPECT_EQ(doc->doctype()->name(), String("html"));
}

TEST_F(HTMLParserTest, Comments) {
    auto doc = parse("<p>Before<!-- comment -->After</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);

    // Should have: text, comment, text
    EXPECT_EQ(p->child_nodes().size(), 3u);
}

TEST_F(HTMLParserTest, Script) {
    auto doc = parse("<script>var x = 1 < 2;</script>");

    ASSERT_NE(doc, nullptr);
    // Script content should be preserved
}

TEST_F(HTMLParserTest, Style) {
    auto doc = parse("<style>.foo { color: red; }</style>");

    ASSERT_NE(doc, nullptr);
    // Style content should be preserved
}

TEST_F(HTMLParserTest, GetElementById) {
    auto doc = parse("<div id=\"test\">Hello</div>");

    ASSERT_NE(doc, nullptr);
    auto* element = doc->get_element_by_id(String("test"));
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->tag_name(), String("div"));
}

TEST_F(HTMLParserTest, GetElementsByTagName) {
    auto doc = parse("<p>1</p><p>2</p><div><p>3</p></div>");

    ASSERT_NE(doc, nullptr);
    auto elements = doc->get_elements_by_tag_name(String("p"));
    EXPECT_EQ(elements.size(), 3u);
}

TEST_F(HTMLParserTest, GetElementsByClassName) {
    auto doc = parse("<div class=\"foo\">1</div><p class=\"foo bar\">2</p><span>3</span>");

    ASSERT_NE(doc, nullptr);
    auto elements = doc->get_elements_by_class_name(String("foo"));
    EXPECT_EQ(elements.size(), 2u);
}
