#include <gtest/gtest.h>
#include "lithium/bindings/dom_bindings.hpp"
#include "lithium/html/parser.hpp"

using namespace lithium;
using namespace lithium::bindings;

class DOMBindingsWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        dom::register_html_fragment_parser(&html::parse_html_fragment);
        bindings.set_document(document);
        bindings.register_all();
    }

    js::VM vm;
    DOMBindings bindings{vm};
    RefPtr<dom::Document> document{make_ref<dom::Document>()};
};

TEST_F(DOMBindingsWrapperTest, InnerHTMLSetterUpdatesDomTree) {
    auto div = document->create_element("div"_s);
    document->append_child(div);

    auto wrapper_value = bindings.wrap_node_for_script(div.get());
    auto* wrapper = wrapper_value.as_object();
    ASSERT_NE(wrapper, nullptr);

    wrapper->set_property("innerHTML"_s, js::Value("<p>Hi</p>"_s));

    ASSERT_EQ(div->child_element_count(), 1u);
    auto* paragraph = div->first_element_child();
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(paragraph->local_name(), "p");
    EXPECT_EQ(paragraph->text_content(), "Hi"_s);
}

TEST_F(DOMBindingsWrapperTest, TextContentSetterReplacesContent) {
    auto div = document->create_element("div"_s);
    document->append_child(div);

    auto wrapper_value = bindings.wrap_node_for_script(div.get());
    auto* wrapper = wrapper_value.as_object();
    ASSERT_NE(wrapper, nullptr);

    wrapper->set_property("textContent"_s, js::Value("Plain text"_s));

    EXPECT_EQ(div->text_content(), "Plain text"_s);
    ASSERT_EQ(div->child_nodes().size(), 1u);
    EXPECT_TRUE(div->first_child()->is_text());
}

TEST_F(DOMBindingsWrapperTest, AttributesCanBeSetThroughWrapper) {
    auto div = document->create_element("div"_s);
    document->append_child(div);

    auto wrapper_value = bindings.wrap_node_for_script(div.get());
    auto* wrapper = wrapper_value.as_object();
    ASSERT_NE(wrapper, nullptr);

    wrapper->set_property("id"_s, js::Value("wrapper-id"_s));
    wrapper->set_property("className"_s, js::Value("a b"_s));

    EXPECT_EQ(div->id(), "wrapper-id"_s);
    EXPECT_EQ(div->class_name(), "a b"_s);
    EXPECT_EQ(wrapper->get_property("id"_s).to_string(), "wrapper-id"_s);
    EXPECT_EQ(wrapper->get_property("className"_s).to_string(), "a b"_s);
}
