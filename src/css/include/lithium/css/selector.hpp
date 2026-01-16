#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>
#include <optional>
#include <memory>

namespace lithium::dom {
class Element;
}

namespace lithium::css {

// ============================================================================
// Selector Components
// ============================================================================

// Simple selectors
struct TypeSelector { String tag_name; };
struct UniversalSelector {};
struct IdSelector { String id; };
struct ClassSelector { String class_name; };

struct AttributeSelector {
    String attribute;
    enum class Matcher {
        Exists,      // [attr]
        Equals,      // [attr=value]
        Includes,    // [attr~=value]
        DashMatch,   // [attr|=value]
        Prefix,      // [attr^=value]
        Suffix,      // [attr$=value]
        Substring,   // [attr*=value]
    };
    Matcher matcher{Matcher::Exists};
    String value;
    bool case_insensitive{false};
};

// Pseudo-classes
struct PseudoClassSelector {
    String name;
    std::optional<String> argument;  // For functional pseudo-classes
};

// Pseudo-elements
struct PseudoElementSelector {
    String name;
};

using SimpleSelector = std::variant<
    TypeSelector,
    UniversalSelector,
    IdSelector,
    ClassSelector,
    AttributeSelector,
    PseudoClassSelector,
    PseudoElementSelector
>;

// ============================================================================
// Compound Selector
// ============================================================================

// A compound selector is a sequence of simple selectors
struct CompoundSelector {
    std::vector<SimpleSelector> selectors;

    [[nodiscard]] bool matches(const dom::Element& element) const;
};

// ============================================================================
// Complex Selector
// ============================================================================

enum class Combinator {
    Descendant,       // space
    Child,            // >
    NextSibling,      // +
    SubsequentSibling // ~
};

struct ComplexSelectorPart {
    CompoundSelector compound;
    std::optional<Combinator> combinator;  // Combinator to next part
};

struct ComplexSelector {
    std::vector<ComplexSelectorPart> parts;

    [[nodiscard]] bool matches(const dom::Element& element) const;
};

// ============================================================================
// Selector List
// ============================================================================

struct SelectorList {
    std::vector<ComplexSelector> selectors;

    [[nodiscard]] bool matches(const dom::Element& element) const;
    [[nodiscard]] static SelectorList parse(const String& selector_text);
};

// ============================================================================
// Specificity
// ============================================================================

struct Specificity {
    u32 a{0};  // ID selectors
    u32 b{0};  // Class selectors, attribute selectors, pseudo-classes
    u32 c{0};  // Type selectors, pseudo-elements

    [[nodiscard]] bool operator<(const Specificity& other) const;
    [[nodiscard]] bool operator<=(const Specificity& other) const;
    [[nodiscard]] bool operator>(const Specificity& other) const;
    [[nodiscard]] bool operator>=(const Specificity& other) const;
    [[nodiscard]] bool operator==(const Specificity& other) const;

    Specificity& operator+=(const Specificity& other);
};

[[nodiscard]] Specificity calculate_specificity(const CompoundSelector& selector);
[[nodiscard]] Specificity calculate_specificity(const ComplexSelector& selector);
[[nodiscard]] Specificity calculate_specificity(const SelectorList& selectors);

// ============================================================================
// Selector Parsing
// ============================================================================

class SelectorParser {
public:
    SelectorParser();

    [[nodiscard]] std::optional<SelectorList> parse(const String& input);
    [[nodiscard]] std::optional<ComplexSelector> parse_complex_selector(const String& input);
    [[nodiscard]] std::optional<CompoundSelector> parse_compound_selector(const String& input);

    [[nodiscard]] const String& error() const { return m_error; }

private:
    std::optional<SelectorList> parse_selector_list();
    std::optional<ComplexSelector> parse_complex_selector();
    std::optional<CompoundSelector> parse_compound_selector();
    std::optional<SimpleSelector> parse_simple_selector();
    std::optional<AttributeSelector> parse_attribute_selector();
    std::optional<PseudoClassSelector> parse_pseudo_class();
    std::optional<PseudoElementSelector> parse_pseudo_element();

    void skip_whitespace();
    [[nodiscard]] bool at_end() const;
    [[nodiscard]] unicode::CodePoint peek(usize offset = 0) const;
    unicode::CodePoint consume();
    bool consume_if(unicode::CodePoint expected);
    String consume_ident();

    String m_input;
    usize m_position{0};
    String m_error;
};

// ============================================================================
// Selector Matching
// ============================================================================

class SelectorMatcher {
public:
    // Match a selector against an element
    [[nodiscard]] static bool matches(const SelectorList& selectors, const dom::Element& element);
    [[nodiscard]] static bool matches(const ComplexSelector& selector, const dom::Element& element);
    [[nodiscard]] static bool matches(const CompoundSelector& selector, const dom::Element& element);
    [[nodiscard]] static bool matches(const SimpleSelector& selector, const dom::Element& element);

    // Query methods
    [[nodiscard]] static dom::Element* query_selector(
        const SelectorList& selectors, const dom::Element& root);
    [[nodiscard]] static std::vector<dom::Element*> query_selector_all(
        const SelectorList& selectors, const dom::Element& root);
};

} // namespace lithium::css
