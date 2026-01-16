#pragma once

#include "tokenizer.hpp"
#include "selector.hpp"
#include "value.hpp"
#include <memory>
#include <vector>

namespace lithium::css {

// ============================================================================
// CSS Rule Types
// ============================================================================

struct Declaration {
    String property;
    std::vector<ComponentValue> value;
    bool important{false};
};

struct DeclarationBlock {
    std::vector<Declaration> declarations;

    [[nodiscard]] std::optional<const Declaration*> get_property(const String& name) const;
};

struct StyleRule {
    SelectorList selectors;
    DeclarationBlock declarations;
};

struct AtRule {
    String name;
    std::vector<ComponentValue> prelude;
    std::optional<std::vector<std::shared_ptr<struct Rule>>> block;
};

using Rule = std::variant<StyleRule, AtRule>;

// ============================================================================
// Stylesheet
// ============================================================================

struct Stylesheet {
    std::vector<Rule> rules;

    // Query all style rules
    [[nodiscard]] std::vector<const StyleRule*> style_rules() const;

    // Query all @import rules
    [[nodiscard]] std::vector<const AtRule*> import_rules() const;

    // Query all @media rules
    [[nodiscard]] std::vector<const AtRule*> media_rules() const;
};

// ============================================================================
// CSS Parser
// ============================================================================

class Parser {
public:
    Parser();

    // Parse a stylesheet
    [[nodiscard]] Stylesheet parse_stylesheet(const String& css);

    // Parse a style attribute value
    [[nodiscard]] DeclarationBlock parse_style_attribute(const String& css);

    // Parse a single declaration
    [[nodiscard]] std::optional<Declaration> parse_declaration(const String& css);

    // Parse a selector
    [[nodiscard]] std::optional<SelectorList> parse_selector(const String& css);

    // Get parse errors
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }

private:
    // Core parsing algorithms (CSS Syntax Module Level 3)
    std::vector<Rule> consume_rule_list(bool top_level);
    std::optional<Rule> consume_rule(bool top_level);
    std::optional<AtRule> consume_at_rule();
    std::optional<StyleRule> consume_qualified_rule();
    DeclarationBlock consume_declaration_block();
    std::vector<Declaration> consume_declaration_list();
    std::optional<Declaration> consume_declaration();
    ComponentValue consume_component_value();
    SimpleBlock consume_simple_block();
    Function consume_function();

    // Token handling
    [[nodiscard]] const Token& current_token() const;
    [[nodiscard]] const Token& peek_token() const;
    void consume_token();
    void reconsume_token();
    [[nodiscard]] bool at_end() const;

    // Error handling
    void parse_error(const String& message);

    // State
    Tokenizer m_tokenizer;
    std::vector<Token> m_tokens;
    usize m_position{0};
    std::vector<String> m_errors;
};

// ============================================================================
// Convenience functions
// ============================================================================

[[nodiscard]] Stylesheet parse_css(const String& css);
[[nodiscard]] DeclarationBlock parse_inline_style(const String& css);

} // namespace lithium::css
