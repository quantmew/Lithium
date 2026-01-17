/**
 * Tokenizer basic functionality test
 */

#include "lithium/html/tokenizer.hpp"
#include <iostream>

using namespace lithium;
using namespace lithium::html;

int main() {
    // Test 1: Simple HTML document
    std::cout << "Test 1: Parsing simple HTML document..." << std::endl;

    String html1 = R"(
        <!DOCTYPE html>
        <html>
            <head>
                <title>Test Page</title>
            </head>
            <body>
                <h1>Hello World</h1>
                <p>This is a test.</p>
            </body>
        </html>
    )"_s;

    Tokenizer tokenizer1;
    tokenizer1.set_input(html1);

    int token_count = 0;
    while (auto token = tokenizer1.next_token()) {
        token_count++;

        if (is_start_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "Start tag: " << tag.name.data() << std::endl;
        } else if (is_end_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "End tag: " << tag.name.data() << std::endl;
        } else if (is_character(*token)) {
            auto& char_token = std::get<CharacterToken>(*token);
            // Skip whitespace for readability
            if (char_token.code_point > ' ') {
                std::cout << "Character: " << static_cast<char>(char_token.code_point) << std::endl;
            }
        } else if (is_comment(*token)) {
            auto& comment = std::get<CommentToken>(*token);
            std::cout << "Comment: " << comment.data.data() << std::endl;
        } else if (is_doctype(*token)) {
            auto& doctype = std::get<DoctypeToken>(*token);
            std::cout << "DOCTYPE: " << doctype.name.data() << std::endl;
        } else if (is_eof(*token)) {
            std::cout << "End of file" << std::endl;
            break;
        }
    }

    std::cout << "Total tokens: " << token_count << std::endl;
    std::cout << std::endl;

    // Test 2: Script tag
    std::cout << "Test 2: Parsing script tag..." << std::endl;

    String html2 = R"(
        <script>
            var x = 42;
            console.log(x);
        </script>
    )"_s;

    Tokenizer tokenizer2;
    tokenizer2.set_input(html2);

    token_count = 0;
    while (auto token = tokenizer2.next_token()) {
        token_count++;

        if (is_start_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "Start tag: " << tag.name.data() << std::endl;
        } else if (is_end_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "End tag: " << tag.name.data() << std::endl;
        } else if (is_character(*token)) {
            auto& char_token = std::get<CharacterToken>(*token);
            std::cout << "Character: " << static_cast<char>(char_token.code_point) << std::endl;
        } else if (is_eof(*token)) {
            std::cout << "End of file" << std::endl;
            break;
        }
    }

    std::cout << "Total tokens: " << token_count << std::endl;
    std::cout << std::endl;

    // Test 3: Character references
    std::cout << "Test 3: Parsing character references..." << std::endl;

    String html3 = R"(
        <p>&amp; &lt; &gt; &quot; &apos;</p>
    )"_s;

    Tokenizer tokenizer3;
    tokenizer3.set_input(html3);

    token_count = 0;
    while (auto token = tokenizer3.next_token()) {
        token_count++;

        if (is_start_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "Start tag: " << tag.name.data() << std::endl;
        } else if (is_end_tag(*token)) {
            auto& tag = std::get<TagToken>(*token);
            std::cout << "End tag: " << tag.name.data() << std::endl;
        } else if (is_character(*token)) {
            auto& char_token = std::get<CharacterToken>(*token);
            std::cout << "Character: " << static_cast<char>(char_token.code_point) << " (U+" << static_cast<int>(char_token.code_point) << ")" << std::endl;
        } else if (is_eof(*token)) {
            std::cout << "End of file" << std::endl;
            break;
        }
    }

    std::cout << "Total tokens: " << token_count << std::endl;

    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}
