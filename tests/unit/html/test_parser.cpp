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

TEST_F(HTMLParserTest, ParagraphsAutoCloseWhenStartingNewParagraph) {
    auto doc = parse("<p>one<p>two");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* first_p = body->first_element_child();
    ASSERT_NE(first_p, nullptr);
    EXPECT_EQ(first_p->tag_name(), String("p"));
    EXPECT_EQ(first_p->text_content(), String("one"));

    ASSERT_TRUE(first_p->next_sibling()->is_element());
    auto* second_p = first_p->next_sibling()->as_element();
    ASSERT_NE(second_p, nullptr);
    EXPECT_EQ(second_p->tag_name(), String("p"));
    EXPECT_EQ(second_p->text_content(), String("two"));
}

TEST_F(HTMLParserTest, ListItemsImplicitlyClosePreviousItem) {
    auto doc = parse("<ul><li>one<li>two</ul>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* ul = body->first_element_child();
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name(), String("ul"));
    ASSERT_EQ(ul->child_nodes().size(), 2u);

    auto* first_li = ul->first_element_child();
    ASSERT_NE(first_li, nullptr);
    EXPECT_EQ(first_li->text_content(), String("one"));

    ASSERT_TRUE(first_li->next_sibling()->is_element());
    auto* second_li = first_li->next_sibling()->as_element();
    ASSERT_NE(second_li, nullptr);
    EXPECT_EQ(second_li->text_content(), String("two"));
}

TEST_F(HTMLParserTest, DescriptionListTermsCloseAutomatically) {
    auto doc = parse("<dl><dt>Term<dd>Def 1<dd>Def 2");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* dl = body->first_element_child();
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name(), String("dl"));
    ASSERT_EQ(dl->child_nodes().size(), 3u);

    auto* dt = dl->first_element_child();
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name(), String("dt"));
    EXPECT_EQ(dt->text_content(), String("Term"));

    ASSERT_TRUE(dt->next_sibling()->is_element());
    auto* dd1 = dt->next_sibling()->as_element();
    ASSERT_NE(dd1, nullptr);
    EXPECT_EQ(dd1->tag_name(), String("dd"));
    EXPECT_EQ(dd1->text_content(), String("Def 1"));

    ASSERT_TRUE(dd1->next_sibling()->is_element());
    auto* dd2 = dd1->next_sibling()->as_element();
    ASSERT_NE(dd2, nullptr);
    EXPECT_EQ(dd2->tag_name(), String("dd"));
    EXPECT_EQ(dd2->text_content(), String("Def 2"));
}

TEST_F(HTMLParserTest, TableCharactersAreFosterParented) {
    auto doc = parse("<div><table>Text<tr><td>Cell</td></tr></table></div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    ASSERT_TRUE(div->first_child()->is_text());
    EXPECT_EQ(div->first_child()->text_content(), String("Text"));

    auto* table = div->first_element_child();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->tag_name(), String("table"));
    // Text should not end up inside the table
    ASSERT_TRUE(table->first_child()->is_element());
    auto* first_in_table = table->first_element_child();
    if (first_in_table->tag_name() == String("tbody")) {
        ASSERT_NE(first_in_table->first_element_child(), nullptr);
        EXPECT_EQ(first_in_table->first_element_child()->tag_name(), String("tr"));
    } else {
        EXPECT_EQ(first_in_table->tag_name(), String("tr"));
    }
}

TEST_F(HTMLParserTest, CharacterReferencesAreResolved) {
    auto doc = parse("<div title=\"Tom &amp; Jerry\">&lt;span&gt;&#x41;&#65;</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->get_attribute("title"_s).value_or(String()), String("Tom & Jerry"));
    EXPECT_EQ(div->text_content(), String("<span>AA"));
}

TEST_F(HTMLParserTest, RcdataAndRawtextEndTagsAllowTrailingWhitespace) {
    auto doc = parse("<textarea>value</textarea   ><script>var x = 1;</script   ><p>after</p>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* textarea = body->first_element_child();
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name(), String("textarea"));
    EXPECT_EQ(textarea->text_content(), String("value"));

    ASSERT_TRUE(textarea->next_sibling()->is_element());
    auto* script = textarea->next_sibling()->as_element();
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name(), String("script"));
    EXPECT_EQ(script->text_content(), String("var x = 1;"));

    ASSERT_TRUE(script->next_sibling()->is_element());
    auto* p = script->next_sibling()->as_element();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("after"));
}
