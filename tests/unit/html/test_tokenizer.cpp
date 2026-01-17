#include <gtest/gtest.h>
#include "lithium/html/tokenizer.hpp"

using namespace lithium;
using namespace lithium::html;

// ============================================================================
// HTML Tokenizer Tests
// ============================================================================

class HTMLTokenizerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const String& html) {
        std::vector<Token> tokens;
        Tokenizer tokenizer;
        tokenizer.set_input(html);
        tokenizer.set_token_callback([&tokens](Token token) {
            tokens.push_back(std::move(token));
        });
        tokenizer.run();
        return tokens;
    }
};

TEST_F(HTMLTokenizerTest, EmptyInput) {
    auto tokens = tokenize("");

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_TRUE(is_eof(tokens[0]));
}

TEST_F(HTMLTokenizerTest, PlainText) {
    auto tokens = tokenize("Hello");

    // Should produce character tokens
    ASSERT_GE(tokens.size(), 2u);  // Characters + EOF
    EXPECT_TRUE(is_eof(tokens.back()));
}

TEST_F(HTMLTokenizerTest, SimpleStartTag) {
    auto tokens = tokenize("<div>");

    ASSERT_GE(tokens.size(), 2u);
    EXPECT_TRUE(is_start_tag(tokens[0]));

    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->name, String("div"));
    EXPECT_FALSE(tag->is_end_tag);
    EXPECT_FALSE(tag->self_closing);
}

TEST_F(HTMLTokenizerTest, SimpleEndTag) {
    auto tokens = tokenize("</div>");

    ASSERT_GE(tokens.size(), 2u);
    EXPECT_TRUE(is_end_tag(tokens[0]));

    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->name, String("div"));
    EXPECT_TRUE(tag->is_end_tag);
}

TEST_F(HTMLTokenizerTest, SelfClosingTag) {
    auto tokens = tokenize("<br/>");

    ASSERT_GE(tokens.size(), 2u);
    EXPECT_TRUE(is_start_tag(tokens[0]));

    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->name, String("br"));
    EXPECT_TRUE(tag->self_closing);
}

TEST_F(HTMLTokenizerTest, TagWithAttribute) {
    auto tokens = tokenize("<div class=\"foo\">");

    ASSERT_GE(tokens.size(), 2u);
    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);

    ASSERT_EQ(tag->attributes.size(), 1u);
    EXPECT_EQ(tag->attributes[0].first, String("class"));
    EXPECT_EQ(tag->attributes[0].second, String("foo"));
}

TEST_F(HTMLTokenizerTest, TagWithMultipleAttributes) {
    auto tokens = tokenize("<input type=\"text\" name=\"field\" value=\"hello\">");

    ASSERT_GE(tokens.size(), 2u);
    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);

    ASSERT_EQ(tag->attributes.size(), 3u);
    EXPECT_EQ(tag->get_attribute(String("type")), String("text"));
    EXPECT_EQ(tag->get_attribute(String("name")), String("field"));
    EXPECT_EQ(tag->get_attribute(String("value")), String("hello"));
}

TEST_F(HTMLTokenizerTest, Comment) {
    auto tokens = tokenize("<!-- This is a comment -->");

    ASSERT_GE(tokens.size(), 2u);
    EXPECT_TRUE(is_comment(tokens[0]));

    auto* comment = std::get_if<CommentToken>(&tokens[0]);
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->data, String(" This is a comment "));
}

TEST_F(HTMLTokenizerTest, Doctype) {
    auto tokens = tokenize("<!DOCTYPE html>");

    ASSERT_GE(tokens.size(), 2u);
    EXPECT_TRUE(is_doctype(tokens[0]));

    auto* doctype = std::get_if<DoctypeToken>(&tokens[0]);
    ASSERT_NE(doctype, nullptr);
    EXPECT_EQ(doctype->name, String("html"));
    EXPECT_FALSE(doctype->force_quirks);
}

TEST_F(HTMLTokenizerTest, MixedContent) {
    auto tokens = tokenize("<p>Hello <b>World</b>!</p>");

    // Should have: <p>, "Hello ", <b>, "World", </b>, "!", </p>, EOF
    EXPECT_TRUE(is_start_tag_named(tokens[0], String("p")));
    EXPECT_TRUE(is_eof(tokens.back()));
}

TEST_F(HTMLTokenizerTest, UnquotedAttribute) {
    auto tokens = tokenize("<div class=foo>");

    ASSERT_GE(tokens.size(), 2u);
    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);

    EXPECT_EQ(tag->get_attribute(String("class")), String("foo"));
}

TEST_F(HTMLTokenizerTest, SingleQuotedAttribute) {
    auto tokens = tokenize("<div class='foo'>");

    ASSERT_GE(tokens.size(), 2u);
    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);

    EXPECT_EQ(tag->get_attribute(String("class")), String("foo"));
}

TEST_F(HTMLTokenizerTest, BooleanAttribute) {
    auto tokens = tokenize("<input disabled>");

    ASSERT_GE(tokens.size(), 2u);
    auto* tag = std::get_if<TagToken>(&tokens[0]);
    ASSERT_NE(tag, nullptr);

    ASSERT_EQ(tag->attributes.size(), 1u);
    EXPECT_EQ(tag->attributes[0].first, String("disabled"));
    EXPECT_EQ(tag->attributes[0].second, String(""));
}

TEST_F(HTMLTokenizerTest, ScriptEscapedEndTagDetected) {
    auto tokens = tokenize("<script><!-- foo --></script>");

    size_t start_count = 0;
    size_t end_count = 0;
    for (const auto& tok : tokens) {
        if (auto* t = std::get_if<TagToken>(&tok)) {
            if (t->name == "script"_s) {
                if (t->is_end_tag) {
                    ++end_count;
                } else {
                    ++start_count;
                }
            }
        }
    }
    EXPECT_EQ(start_count, 1u);
    EXPECT_EQ(end_count, 1u);
}

TEST_F(HTMLTokenizerTest, ScriptDoubleEscapedDoesNotCloseEarly) {
    auto tokens = tokenize("<script><!--<script></script>--></script>");

    size_t start_count = 0;
    size_t end_count = 0;
    for (const auto& tok : tokens) {
        if (auto* t = std::get_if<TagToken>(&tok)) {
            if (t->name == "script"_s) {
                if (t->is_end_tag) {
                    ++end_count;
                } else {
                    ++start_count;
                }
            }
        }
    }

    EXPECT_EQ(start_count, 1u);
    EXPECT_EQ(end_count, 1u);
}
