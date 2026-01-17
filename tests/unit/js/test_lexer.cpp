#include <gtest/gtest.h>
#include "lithium/js/lexer.hpp"

using namespace lithium;
using namespace lithium::js;

// ============================================================================
// JavaScript Lexer Tests
// ============================================================================

class JSLexerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const String& source) {
        std::vector<Token> tokens;
        Lexer lexer;
        lexer.set_input(source);
        lexer.set_allow_regexp(false);

        while (true) {
            Token token = lexer.next_token();
            tokens.push_back(token);
            if (token.type == TokenType::EndOfFile) {
                break;
            }
        }

        return tokens;
    }
};

TEST_F(JSLexerTest, EmptyInput) {
    auto tokens = tokenize("");

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::EndOfFile);
}

TEST_F(JSLexerTest, Identifier) {
    auto tokens = tokenize("foo");

    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, String("foo"));
}

TEST_F(JSLexerTest, Keywords) {
    auto tokens = tokenize("let const var function if else");

    EXPECT_EQ(tokens[0].type, TokenType::Let);
    EXPECT_EQ(tokens[1].type, TokenType::Const);
    EXPECT_EQ(tokens[2].type, TokenType::Var);
    EXPECT_EQ(tokens[3].type, TokenType::Function);
    EXPECT_EQ(tokens[4].type, TokenType::If);
    EXPECT_EQ(tokens[5].type, TokenType::Else);
}

TEST_F(JSLexerTest, Numbers) {
    auto tokens = tokenize("42 3.14 0xFF 1e10");

    EXPECT_EQ(tokens[0].type, TokenType::Number);
    EXPECT_DOUBLE_EQ(tokens[0].number_value, 42.0);

    EXPECT_EQ(tokens[1].type, TokenType::Number);
    EXPECT_DOUBLE_EQ(tokens[1].number_value, 3.14);

    EXPECT_EQ(tokens[2].type, TokenType::Number);
    EXPECT_DOUBLE_EQ(tokens[2].number_value, 255.0);

    EXPECT_EQ(tokens[3].type, TokenType::Number);
    EXPECT_DOUBLE_EQ(tokens[3].number_value, 1e10);
}

TEST_F(JSLexerTest, Strings) {
    auto tokens = tokenize("\"hello\" 'world'");

    EXPECT_EQ(tokens[0].type, TokenType::String);
    EXPECT_EQ(tokens[0].value, String("hello"));

    EXPECT_EQ(tokens[1].type, TokenType::String);
    EXPECT_EQ(tokens[1].value, String("world"));
}

TEST_F(JSLexerTest, StringEscapes) {
    auto tokens = tokenize("\"hello\\nworld\"");

    EXPECT_EQ(tokens[0].type, TokenType::String);
    EXPECT_EQ(tokens[0].value, String("hello\nworld"));
}

TEST_F(JSLexerTest, Punctuators) {
    auto tokens = tokenize("+ - * / % = == === != !== < > <= >= && || !");

    EXPECT_EQ(tokens[0].type, TokenType::Plus);
    EXPECT_EQ(tokens[1].type, TokenType::Minus);
    EXPECT_EQ(tokens[2].type, TokenType::Star);
    EXPECT_EQ(tokens[3].type, TokenType::Slash);
    EXPECT_EQ(tokens[4].type, TokenType::Percent);
    EXPECT_EQ(tokens[5].type, TokenType::Assign);
    EXPECT_EQ(tokens[6].type, TokenType::Equal);
    EXPECT_EQ(tokens[7].type, TokenType::StrictEqual);
    EXPECT_EQ(tokens[8].type, TokenType::NotEqual);
    EXPECT_EQ(tokens[9].type, TokenType::StrictNotEqual);
    EXPECT_EQ(tokens[10].type, TokenType::LessThan);
    EXPECT_EQ(tokens[11].type, TokenType::GreaterThan);
    EXPECT_EQ(tokens[12].type, TokenType::LessEqual);
    EXPECT_EQ(tokens[13].type, TokenType::GreaterEqual);
    EXPECT_EQ(tokens[14].type, TokenType::AmpersandAmpersand);
    EXPECT_EQ(tokens[15].type, TokenType::PipePipe);
    EXPECT_EQ(tokens[16].type, TokenType::Exclamation);
}

TEST_F(JSLexerTest, Brackets) {
    auto tokens = tokenize("{ } [ ] ( )");

    EXPECT_EQ(tokens[0].type, TokenType::OpenBrace);
    EXPECT_EQ(tokens[1].type, TokenType::CloseBrace);
    EXPECT_EQ(tokens[2].type, TokenType::OpenBracket);
    EXPECT_EQ(tokens[3].type, TokenType::CloseBracket);
    EXPECT_EQ(tokens[4].type, TokenType::OpenParen);
    EXPECT_EQ(tokens[5].type, TokenType::CloseParen);
}

TEST_F(JSLexerTest, ArrowFunction) {
    auto tokens = tokenize("x => x * 2");

    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].type, TokenType::Arrow);
    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[3].type, TokenType::Star);
    EXPECT_EQ(tokens[4].type, TokenType::Number);
}

TEST_F(JSLexerTest, Comments) {
    auto tokens = tokenize("a // comment\nb /* block */ c");

    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, String("a"));
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, String("b"));
    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].value, String("c"));
}

TEST_F(JSLexerTest, LineTerminator) {
    Lexer lexer;
    lexer.set_input("a\nb"_s);

    Token a = lexer.next_token();
    Token b = lexer.next_token();

    EXPECT_EQ(a.value, String("a"));
    EXPECT_EQ(b.value, String("b"));
    EXPECT_TRUE(b.preceded_by_line_terminator);
}

TEST_F(JSLexerTest, TemplateLiteral) {
    auto tokens = tokenize("`hello`");

    EXPECT_EQ(tokens[0].type, TokenType::NoSubstitutionTemplate);
    EXPECT_EQ(tokens[0].value, String("hello"));
}

TEST_F(JSLexerTest, BooleanLiterals) {
    auto tokens = tokenize("true false");

    EXPECT_EQ(tokens[0].type, TokenType::True);
    EXPECT_EQ(tokens[1].type, TokenType::False);
}

TEST_F(JSLexerTest, NullLiteral) {
    auto tokens = tokenize("null");

    EXPECT_EQ(tokens[0].type, TokenType::Null);
}

TEST_F(JSLexerTest, ClassKeyword) {
    auto tokens = tokenize("class Foo extends Bar");

    EXPECT_EQ(tokens[0].type, TokenType::Class);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].type, TokenType::Extends);
    EXPECT_EQ(tokens[3].type, TokenType::Identifier);
}

TEST_F(JSLexerTest, DistinguishesRegExpFromDivisionWithContext) {
    Lexer lexer;
    lexer.set_input("/abc/i 1 / 2"_s);
    lexer.set_allow_regexp(true);

    Token first = lexer.next_token();
    EXPECT_EQ(first.type, TokenType::RegExp);
    EXPECT_EQ(first.value, String("abc"));
    EXPECT_EQ(first.regex_flags, String("i"));

    lexer.set_allow_regexp(false);
    Token number = lexer.next_token();
    EXPECT_EQ(number.type, TokenType::Number);
    Token slash = lexer.next_token();
    EXPECT_EQ(slash.type, TokenType::Slash);
}
