#include <gtest/gtest.h>
#include "lithium/css/tokenizer.hpp"

using namespace lithium;
using namespace lithium::css;

static std::vector<NumberToken> collect_numbers(const std::vector<Token>& tokens) {
    std::vector<NumberToken> numbers;
    for (const auto& token : tokens) {
        if (auto* num = std::get_if<NumberToken>(&token)) {
            numbers.push_back(*num);
        }
    }
    return numbers;
}

TEST(CSSTokenizerTest, ParsesScientificAndNegativeNumbers) {
    Tokenizer tokenizer;
    tokenizer.set_input("1e2 10 10.5 -3.0e-1"_s);
    auto tokens = tokenizer.tokenize();
    auto numbers = collect_numbers(tokens);

    ASSERT_EQ(numbers.size(), 4u);
    EXPECT_DOUBLE_EQ(numbers[0].value, 100.0);
    EXPECT_FALSE(numbers[0].is_integer);

    EXPECT_DOUBLE_EQ(numbers[1].value, 10.0);
    EXPECT_TRUE(numbers[1].is_integer);

    EXPECT_DOUBLE_EQ(numbers[2].value, 10.5);
    EXPECT_FALSE(numbers[2].is_integer);

    EXPECT_NEAR(numbers[3].value, -0.3, 1e-9);
    EXPECT_FALSE(numbers[3].is_integer);
}
