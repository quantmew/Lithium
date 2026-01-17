#include <gtest/gtest.h>
#include "lithium/dom/document.hpp"
#include "lithium/dom/element.hpp"
#include "lithium/html/parser.hpp"

using namespace lithium;

class DOMInnerHTMLTest : public ::testing::Test {
protected:
    void SetUp() override {
        dom::register_html_fragment_parser(&html::parse_html_fragment);
        document = make_ref<dom::Document>();
    }

    RefPtr<dom::Document> document;
};

TEST_F(DOMInnerHTMLTest, ParsesMarkupAndAdoptsIntoDocument) {
    auto container = document->create_element("div"_s);
    document->append_child(container);

    container->set_inner_html("<p id=\"greeting\">Hello <span>World</span></p>");

    ASSERT_EQ(container->child_element_count(), 1u);

    auto* paragraph = container->first_element_child();
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(paragraph->local_name(), "p");
    EXPECT_EQ(paragraph->owner_document(), document.get());
    EXPECT_EQ(paragraph->id(), "greeting"_s);

    auto* span = paragraph->first_element_child();
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->local_name(), "span");
    EXPECT_EQ(span->owner_document(), document.get());
    EXPECT_EQ(paragraph->text_content(), "Hello World"_s);
}

TEST_F(DOMInnerHTMLTest, ContextualParsingInTablesCreatesTbody) {
    auto table = document->create_element("table"_s);
    document->append_child(table);

    table->set_inner_html("<tr><td>Cell</td></tr>");

    auto* tbody = table->first_element_child();
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(tbody->local_name(), "tbody");
    EXPECT_EQ(tbody->owner_document(), document.get());

    auto* row = tbody->first_element_child();
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->local_name(), "tr");
    EXPECT_EQ(row->owner_document(), document.get());

    auto* cell = row->first_element_child();
    ASSERT_NE(cell, nullptr);
    EXPECT_EQ(cell->local_name(), "td");
    EXPECT_EQ(cell->text_content(), "Cell");
    EXPECT_EQ(cell->owner_document(), document.get());
}

TEST_F(DOMInnerHTMLTest, FallsBackToTextWhenParserUnavailable) {
    dom::register_html_fragment_parser(nullptr);

    auto container = document->create_element("div"_s);
    container->set_inner_html("<em>raw</em>");

    ASSERT_EQ(container->child_nodes().size(), 1u);
    auto* text_node = container->first_child();
    ASSERT_NE(text_node, nullptr);
    EXPECT_TRUE(text_node->is_text());
    EXPECT_EQ(container->text_content(), "<em>raw</em>"_s);

    dom::register_html_fragment_parser(&html::parse_html_fragment);
}
