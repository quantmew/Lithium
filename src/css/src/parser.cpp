/**
 * CSS Parser implementation
 * Following CSS Syntax Module Level 3
 */

#include "lithium/css/parser.hpp"
#include <algorithm>

namespace lithium::css {

// Helper to convert number to string
static String number_to_string(f64 value) {
    StringBuilder sb;
    sb.append(value);
    return sb.build();
}

// ============================================================================
// Stylesheet
// ============================================================================

std::vector<const StyleRule*> Stylesheet::style_rules() const {
    std::vector<const StyleRule*> result;
    for (const auto& rule : rules) {
        if (rule.is<StyleRule>()) {
            result.push_back(&rule.get<StyleRule>());
        }
    }
    return result;
}

std::vector<const AtRule*> Stylesheet::import_rules() const {
    std::vector<const AtRule*> result;
    for (const auto& rule : rules) {
        if (rule.is<AtRule>()) {
            const auto& at_rule = rule.get<AtRule>();
            if (at_rule.name.to_lowercase() == "import"_s) {
                result.push_back(&at_rule);
            }
        }
    }
    return result;
}

std::vector<const AtRule*> Stylesheet::media_rules() const {
    std::vector<const AtRule*> result;
    for (const auto& rule : rules) {
        if (rule.is<AtRule>()) {
            const auto& at_rule = rule.get<AtRule>();
            if (at_rule.name.to_lowercase() == "media"_s) {
                result.push_back(&at_rule);
            }
        }
    }
    return result;
}

// ============================================================================
// DeclarationBlock
// ============================================================================

std::optional<const Declaration*> DeclarationBlock::get_property(const String& name) const {
    auto lower_name = name.to_lowercase();
    for (const auto& decl : declarations) {
        if (decl.property.to_lowercase() == lower_name) {
            return &decl;
        }
    }
    return std::nullopt;
}

// ============================================================================
// Parser implementation
// ============================================================================

Parser::Parser() = default;

Stylesheet Parser::parse_stylesheet(const String& css) {
    m_tokenizer.set_input(css);
    m_tokens = m_tokenizer.tokenize();
    m_position = 0;
    m_errors.clear();

    Stylesheet stylesheet;
    stylesheet.rules = consume_rule_list(true);
    return stylesheet;
}

DeclarationBlock Parser::parse_style_attribute(const String& css) {
    m_tokenizer.set_input(css);
    m_tokens = m_tokenizer.tokenize();
    m_position = 0;
    m_errors.clear();

    DeclarationBlock block;
    block.declarations = consume_declaration_list();
    return block;
}

std::optional<Declaration> Parser::parse_declaration(const String& css) {
    m_tokenizer.set_input(css);
    m_tokens = m_tokenizer.tokenize();
    m_position = 0;
    m_errors.clear();

    return consume_declaration();
}

std::optional<SelectorList> Parser::parse_selector(const String& css) {
    SelectorParser parser;
    return parser.parse(css);
}

const Token& Parser::current_token() const {
    if (m_position >= m_tokens.size()) {
        static EOFToken eof;
        return eof;
    }
    return m_tokens[m_position];
}

const Token& Parser::peek_token() const {
    if (m_position + 1 >= m_tokens.size()) {
        static EOFToken eof;
        return eof;
    }
    return m_tokens[m_position + 1];
}

void Parser::consume_token() {
    if (m_position < m_tokens.size()) {
        ++m_position;
    }
}

void Parser::reconsume_token() {
    if (m_position > 0) {
        --m_position;
    }
}

bool Parser::at_end() const {
    return m_position >= m_tokens.size() ||
           std::holds_alternative<EOFToken>(current_token());
}

void Parser::parse_error(const String& message) {
    m_errors.push_back(message);
}

std::vector<Rule> Parser::consume_rule_list(bool top_level, bool stop_on_close_curly) {
    std::vector<Rule> rules;

    while (!at_end()) {
        const auto& token = current_token();

        // When parsing inside a block, bail out once we see the closing brace.
        if (stop_on_close_curly && std::holds_alternative<CloseCurlyToken>(token)) {
            consume_token();
            break;
        }

        if (std::holds_alternative<WhitespaceToken>(token)) {
            consume_token();
            continue;
        }

        if (std::holds_alternative<EOFToken>(token)) {
            break;
        }

        if (std::holds_alternative<CDOToken>(token) ||
            std::holds_alternative<CDCToken>(token)) {
            if (top_level) {
                consume_token();
                continue;
            }
            if (auto rule = consume_qualified_rule()) {
                rules.push_back(*rule);
            }
            continue;
        }

        if (auto rule = consume_rule(top_level, stop_on_close_curly)) {
            rules.push_back(*rule);
        }
    }

    return rules;
}

std::optional<Rule> Parser::consume_rule(bool top_level, bool stop_on_close_curly) {
    while (!at_end() && std::holds_alternative<WhitespaceToken>(current_token())) {
        consume_token();
    }

    if (at_end()) return std::nullopt;

    if (stop_on_close_curly && std::holds_alternative<CloseCurlyToken>(current_token())) {
        consume_token();
        return std::nullopt;
    }

    if (std::holds_alternative<AtKeywordToken>(current_token())) {
        return consume_at_rule();
    }

    return consume_qualified_rule();
}

std::optional<AtRule> Parser::consume_at_rule() {
    auto& at_token = std::get<AtKeywordToken>(current_token());
    AtRule rule;
    rule.name = at_token.value;
    consume_token();

    auto lower_name = rule.name.to_lowercase();
    auto at_rule_allows_rule_list = [&](const String& name) {
        return name == "media"_s ||
               name == "supports"_s ||
               name == "layer"_s ||
               name == "container"_s ||
               name == "scope"_s ||
               name == "document"_s ||
               name == "keyframes"_s ||
               name == "-webkit-keyframes"_s ||
               name == "-moz-keyframes"_s ||
               name == "-o-keyframes"_s;
    };

    // Consume prelude
    while (!at_end()) {
        const auto& token = current_token();

        if (std::holds_alternative<SemicolonToken>(token)) {
            consume_token();
            return rule;
        }

        if (std::holds_alternative<EOFToken>(token)) {
            parse_error("Unexpected EOF in at-rule"_s);
            return rule;
        }

        if (std::holds_alternative<OpenCurlyToken>(token)) {
            consume_token(); // consume {

            if (at_rule_allows_rule_list(lower_name)) {
                auto inner_rules = consume_rule_list(false, true);
                std::vector<std::shared_ptr<RuleVariant>> nested;
                nested.reserve(inner_rules.size());
                for (auto& inner : inner_rules) {
                    nested.push_back(std::make_shared<RuleVariant>(std::move(inner)));
                }
                rule.nested_rules = std::move(nested);
            } else {
                rule.declarations = consume_declaration_block();
            }
            return rule;
        }

        rule.prelude.push_back(consume_component_value());
    }

    return rule;
}

std::optional<StyleRule> Parser::consume_qualified_rule() {
    StyleRule rule;
    StringBuilder selector_text;

    // Consume prelude (selector)
    while (!at_end()) {
        const auto& token = current_token();

        if (std::holds_alternative<EOFToken>(token)) {
            parse_error("Unexpected EOF in qualified rule"_s);
            return std::nullopt;
        }

        if (std::holds_alternative<OpenCurlyToken>(token)) {
            // Parse selector
            SelectorParser parser;
            if (auto selectors = parser.parse(selector_text.build())) {
                rule.selectors = *selectors;
            } else {
                parse_error("Invalid selector: "_s + parser.error());
            }

            // Consume declaration block
            consume_token(); // consume {
            rule.declarations = consume_declaration_block();
            return rule;
        }

        // Accumulate selector text
        if (auto* ident = std::get_if<IdentToken>(&token)) {
            selector_text.append(ident->value);
        } else if (auto* hash = std::get_if<HashToken>(&token)) {
            selector_text.append('#');
            selector_text.append(hash->value);
        } else if (auto* delim = std::get_if<DelimToken>(&token)) {
            selector_text.append(delim->value);
        } else if (std::holds_alternative<WhitespaceToken>(token)) {
            selector_text.append(' ');
        } else if (std::holds_alternative<ColonToken>(token)) {
            selector_text.append(':');
        } else if (std::holds_alternative<OpenSquareToken>(token)) {
            selector_text.append('[');
        } else if (std::holds_alternative<CloseSquareToken>(token)) {
            selector_text.append(']');
        } else if (std::holds_alternative<OpenParenToken>(token)) {
            selector_text.append('(');
        } else if (std::holds_alternative<CloseParenToken>(token)) {
            selector_text.append(')');
        } else if (auto* string_tok = std::get_if<StringToken>(&token)) {
            selector_text.append('"');
            selector_text.append(string_tok->value);
            selector_text.append('"');
        } else if (auto* num = std::get_if<NumberToken>(&token)) {
            selector_text.append(num->value);  // StringBuilder::append(f64)
        } else if (std::holds_alternative<CommaToken>(token)) {
            selector_text.append(',');
        }

        consume_token();
    }

    return std::nullopt;
}

DeclarationBlock Parser::consume_declaration_block() {
    DeclarationBlock block;
    block.declarations = consume_declaration_list();

    // Consume closing brace if present
    if (!at_end() && std::holds_alternative<CloseCurlyToken>(current_token())) {
        consume_token();
    }

    return block;
}

std::vector<Declaration> Parser::consume_declaration_list() {
    std::vector<Declaration> declarations;

    while (!at_end()) {
        const auto& token = current_token();

        if (std::holds_alternative<WhitespaceToken>(token) ||
            std::holds_alternative<SemicolonToken>(token)) {
            consume_token();
            continue;
        }

        if (std::holds_alternative<EOFToken>(token) ||
            std::holds_alternative<CloseCurlyToken>(token)) {
            break;
        }

        if (std::holds_alternative<AtKeywordToken>(token)) {
            // At-rule in declaration block (not common, but valid)
            consume_at_rule();
            continue;
        }

        if (std::holds_alternative<IdentToken>(token)) {
            if (auto decl = consume_declaration()) {
                declarations.push_back(*decl);
            }
        } else {
            // Skip until ; or }
            while (!at_end() &&
                   !std::holds_alternative<SemicolonToken>(current_token()) &&
                   !std::holds_alternative<CloseCurlyToken>(current_token())) {
                consume_token();
            }
        }
    }

    return declarations;
}

std::optional<Declaration> Parser::consume_declaration() {
    Declaration decl;

    // Property name
    if (auto* ident = std::get_if<IdentToken>(&current_token())) {
        decl.property = ident->value;
        consume_token();
    } else {
        return std::nullopt;
    }

    // Skip whitespace
    while (!at_end() && std::holds_alternative<WhitespaceToken>(current_token())) {
        consume_token();
    }

    // Colon
    if (!std::holds_alternative<ColonToken>(current_token())) {
        parse_error("Expected ':' in declaration"_s);
        return std::nullopt;
    }
    consume_token();

    // Skip whitespace
    while (!at_end() && std::holds_alternative<WhitespaceToken>(current_token())) {
        consume_token();
    }

    // Value
    while (!at_end() &&
           !std::holds_alternative<SemicolonToken>(current_token()) &&
           !std::holds_alternative<CloseCurlyToken>(current_token())) {
        // Check for !important
        if (auto* delim = std::get_if<DelimToken>(&current_token())) {
            if (delim->value == '!') {
                consume_token();
                // Skip whitespace
                while (!at_end() && std::holds_alternative<WhitespaceToken>(current_token())) {
                    consume_token();
                }
                if (auto* ident = std::get_if<IdentToken>(&current_token())) {
                    if (ident->value.to_lowercase() == "important"_s) {
                        decl.important = true;
                        consume_token();
                        continue;
                    }
                }
            }
        }

        decl.value.push_back(consume_component_value());
    }

    // Remove trailing whitespace from value
    while (!decl.value.empty()) {
        if (auto* preserved = std::get_if<PreservedToken>(&decl.value.back())) {
            if (preserved->value.trim().empty()) {
                decl.value.pop_back();
                continue;
            }
        }
        break;
    }

    return decl;
}

ComponentValue Parser::consume_component_value() {
    const auto& token = current_token();

    if (std::holds_alternative<OpenCurlyToken>(token) ||
        std::holds_alternative<OpenSquareToken>(token) ||
        std::holds_alternative<OpenParenToken>(token)) {
        return std::make_shared<SimpleBlock>(consume_simple_block());
    }

    if (std::holds_alternative<FunctionToken>(token)) {
        return std::make_shared<Function>(consume_function());
    }

    // Preserved token
    PreservedToken preserved;
    if (auto* ident = std::get_if<IdentToken>(&token)) {
        preserved.value = ident->value;
    } else if (auto* string_tok = std::get_if<StringToken>(&token)) {
        preserved.value = string_tok->value;
    } else if (auto* num = std::get_if<NumberToken>(&token)) {
        preserved.value = number_to_string(num->value);
    } else if (auto* dim = std::get_if<DimensionToken>(&token)) {
        preserved.value = number_to_string(dim->value) + dim->unit;
    } else if (auto* pct = std::get_if<PercentageToken>(&token)) {
        preserved.value = number_to_string(pct->value) + "%"_s;
    } else if (auto* hash = std::get_if<HashToken>(&token)) {
        preserved.value = "#"_s + hash->value;
    } else if (auto* delim = std::get_if<DelimToken>(&token)) {
        preserved.value = String(1, static_cast<char>(delim->value));
    } else if (std::holds_alternative<WhitespaceToken>(token)) {
        preserved.value = " "_s;
    } else if (std::holds_alternative<CommaToken>(token)) {
        preserved.value = ","_s;
    } else if (std::holds_alternative<ColonToken>(token)) {
        preserved.value = ":"_s;
    } else if (auto* url = std::get_if<UrlToken>(&token)) {
        preserved.value = "url("_s + url->value + ")"_s;
    }

    consume_token();
    return preserved;
}

SimpleBlock Parser::consume_simple_block() {
    SimpleBlock block;

    const auto& start = current_token();
    if (std::holds_alternative<OpenCurlyToken>(start)) {
        block.associated_token = '{';
    } else if (std::holds_alternative<OpenSquareToken>(start)) {
        block.associated_token = '[';
    } else if (std::holds_alternative<OpenParenToken>(start)) {
        block.associated_token = '(';
    }

    unicode::CodePoint ending;
    if (block.associated_token == '{') ending = '}';
    else if (block.associated_token == '[') ending = ']';
    else ending = ')';

    consume_token(); // opening token

    while (!at_end()) {
        const auto& token = current_token();

        bool is_ending = false;
        if (ending == '}' && std::holds_alternative<CloseCurlyToken>(token)) is_ending = true;
        if (ending == ']' && std::holds_alternative<CloseSquareToken>(token)) is_ending = true;
        if (ending == ')' && std::holds_alternative<CloseParenToken>(token)) is_ending = true;

        if (is_ending) {
            consume_token();
            return block;
        }

        block.value.push_back(consume_component_value());
    }

    return block;
}

Function Parser::consume_function() {
    Function func;

    if (auto* func_token = std::get_if<FunctionToken>(&current_token())) {
        func.name = func_token->value;
    }

    consume_token(); // function token

    while (!at_end()) {
        if (std::holds_alternative<CloseParenToken>(current_token())) {
            consume_token();
            return func;
        }

        func.value.push_back(consume_component_value());
    }

    return func;
}

// ============================================================================
// Convenience functions
// ============================================================================

Stylesheet parse_css(const String& css) {
    Parser parser;
    return parser.parse_stylesheet(css);
}

DeclarationBlock parse_inline_style(const String& css) {
    Parser parser;
    return parser.parse_style_attribute(css);
}

} // namespace lithium::css
