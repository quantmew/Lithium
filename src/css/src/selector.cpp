/**
 * CSS Selector implementation
 */

#include "lithium/css/selector.hpp"
#include "lithium/dom/element.hpp"
#include <algorithm>
#include <cctype>

namespace lithium::css {

// ============================================================================
// Specificity
// ============================================================================

bool Specificity::operator<(const Specificity& other) const {
    if (a != other.a) return a < other.a;
    if (b != other.b) return b < other.b;
    return c < other.c;
}

bool Specificity::operator<=(const Specificity& other) const {
    return *this < other || *this == other;
}

bool Specificity::operator>(const Specificity& other) const {
    return other < *this;
}

bool Specificity::operator>=(const Specificity& other) const {
    return !(*this < other);
}

bool Specificity::operator==(const Specificity& other) const {
    return a == other.a && b == other.b && c == other.c;
}

Specificity& Specificity::operator+=(const Specificity& other) {
    a += other.a;
    b += other.b;
    c += other.c;
    return *this;
}

Specificity calculate_specificity(const CompoundSelector& selector) {
    Specificity spec;
    for (const auto& simple : selector.selectors) {
        std::visit([&spec](const auto& sel) {
            using T = std::decay_t<decltype(sel)>;
            if constexpr (std::is_same_v<T, IdSelector>) {
                spec.a++;
            } else if constexpr (std::is_same_v<T, ClassSelector> ||
                                 std::is_same_v<T, AttributeSelector> ||
                                 std::is_same_v<T, PseudoClassSelector>) {
                spec.b++;
            } else if constexpr (std::is_same_v<T, TypeSelector> ||
                                 std::is_same_v<T, PseudoElementSelector>) {
                spec.c++;
            }
            // UniversalSelector doesn't add specificity
        }, simple);
    }
    return spec;
}

Specificity calculate_specificity(const ComplexSelector& selector) {
    Specificity spec;
    for (const auto& part : selector.parts) {
        spec += calculate_specificity(part.compound);
    }
    return spec;
}

Specificity calculate_specificity(const SelectorList& selectors) {
    Specificity max;
    for (const auto& sel : selectors.selectors) {
        Specificity spec = calculate_specificity(sel);
        if (spec > max) {
            max = spec;
        }
    }
    return max;
}

// ============================================================================
// Selector matching
// ============================================================================

bool CompoundSelector::matches(const dom::Element& element) const {
    return SelectorMatcher::matches(*this, element);
}

bool ComplexSelector::matches(const dom::Element& element) const {
    return SelectorMatcher::matches(*this, element);
}

bool SelectorList::matches(const dom::Element& element) const {
    return SelectorMatcher::matches(*this, element);
}

SelectorList SelectorList::parse(const String& selector_text) {
    SelectorParser parser;
    auto result = parser.parse(selector_text);
    return result.value_or(SelectorList{});
}

// ============================================================================
// SelectorParser implementation
// ============================================================================

SelectorParser::SelectorParser() = default;

std::optional<SelectorList> SelectorParser::parse(const String& input) {
    m_input = input;
    m_position = 0;
    m_error.clear();
    return parse_selector_list();
}

std::optional<ComplexSelector> SelectorParser::parse_complex_selector(const String& input) {
    m_input = input;
    m_position = 0;
    m_error.clear();
    return parse_complex_selector();
}

std::optional<CompoundSelector> SelectorParser::parse_compound_selector(const String& input) {
    m_input = input;
    m_position = 0;
    m_error.clear();
    return parse_compound_selector();
}

std::optional<SelectorList> SelectorParser::parse_selector_list() {
    SelectorList list;

    skip_whitespace();

    while (!at_end()) {
        auto selector = parse_complex_selector();
        if (!selector) {
            return std::nullopt;
        }
        list.selectors.push_back(*selector);

        skip_whitespace();
        if (!at_end() && peek() == ',') {
            consume(); // comma
            skip_whitespace();
        } else {
            break;
        }
    }

    if (list.selectors.empty()) {
        m_error = "Empty selector"_s;
        return std::nullopt;
    }

    return list;
}

std::optional<ComplexSelector> SelectorParser::parse_complex_selector() {
    ComplexSelector selector;

    auto compound = parse_compound_selector();
    if (!compound) {
        return std::nullopt;
    }

    ComplexSelectorPart part;
    part.compound = *compound;

    while (true) {
        skip_whitespace();
        if (at_end() || peek() == ',') {
            selector.parts.push_back(part);
            break;
        }

        // Check for combinator
        std::optional<Combinator> combinator;
        unicode::CodePoint cp = peek();

        if (cp == '>') {
            consume();
            combinator = Combinator::Child;
            skip_whitespace();
        } else if (cp == '+') {
            consume();
            combinator = Combinator::NextSibling;
            skip_whitespace();
        } else if (cp == '~') {
            consume();
            combinator = Combinator::SubsequentSibling;
            skip_whitespace();
        } else if (!at_end() && cp != ',' && cp != '{') {
            // Descendant combinator (whitespace)
            combinator = Combinator::Descendant;
        } else {
            selector.parts.push_back(part);
            break;
        }

        part.combinator = combinator;
        selector.parts.push_back(part);

        // Parse next compound
        compound = parse_compound_selector();
        if (!compound) {
            if (combinator) {
                m_error = "Expected selector after combinator"_s;
                return std::nullopt;
            }
            break;
        }

        part = ComplexSelectorPart();
        part.compound = *compound;
    }

    if (selector.parts.empty()) {
        m_error = "Empty complex selector"_s;
        return std::nullopt;
    }

    return selector;
}

std::optional<CompoundSelector> SelectorParser::parse_compound_selector() {
    CompoundSelector selector;

    while (!at_end()) {
        auto simple = parse_simple_selector();
        if (!simple) {
            break;
        }
        selector.selectors.push_back(*simple);
    }

    if (selector.selectors.empty()) {
        return std::nullopt;
    }

    return selector;
}

std::optional<SimpleSelector> SelectorParser::parse_simple_selector() {
    if (at_end()) return std::nullopt;

    unicode::CodePoint cp = peek();

    // Universal selector
    if (cp == '*') {
        consume();
        return UniversalSelector{};
    }

    // ID selector
    if (cp == '#') {
        consume();
        String id = consume_ident();
        if (id.empty()) {
            m_error = "Expected identifier after #"_s;
            return std::nullopt;
        }
        return IdSelector{id};
    }

    // Class selector
    if (cp == '.') {
        consume();
        String class_name = consume_ident();
        if (class_name.empty()) {
            m_error = "Expected identifier after ."_s;
            return std::nullopt;
        }
        return ClassSelector{class_name};
    }

    // Attribute selector
    if (cp == '[') {
        return parse_attribute_selector();
    }

    // Pseudo-class or pseudo-element
    if (cp == ':') {
        consume();
        if (peek() == ':') {
            consume();
            return parse_pseudo_element();
        }
        return parse_pseudo_class();
    }

    // Type selector
    if (std::isalpha(cp) || cp == '_' || cp > 0x7F) {
        String tag_name = consume_ident();
        if (!tag_name.empty()) {
            return TypeSelector{tag_name};
        }
    }

    return std::nullopt;
}

std::optional<AttributeSelector> SelectorParser::parse_attribute_selector() {
    consume(); // [

    skip_whitespace();

    AttributeSelector sel;
    sel.attribute = consume_ident();
    if (sel.attribute.empty()) {
        m_error = "Expected attribute name"_s;
        return std::nullopt;
    }

    skip_whitespace();

    if (peek() == ']') {
        consume();
        sel.matcher = AttributeSelector::Matcher::Exists;
        return sel;
    }

    // Matcher
    unicode::CodePoint first = consume();
    unicode::CodePoint second = peek();

    if (first == '=' && second != '=') {
        sel.matcher = AttributeSelector::Matcher::Equals;
    } else if (first == '~' && second == '=') {
        consume();
        sel.matcher = AttributeSelector::Matcher::Includes;
    } else if (first == '|' && second == '=') {
        consume();
        sel.matcher = AttributeSelector::Matcher::DashMatch;
    } else if (first == '^' && second == '=') {
        consume();
        sel.matcher = AttributeSelector::Matcher::Prefix;
    } else if (first == '$' && second == '=') {
        consume();
        sel.matcher = AttributeSelector::Matcher::Suffix;
    } else if (first == '*' && second == '=') {
        consume();
        sel.matcher = AttributeSelector::Matcher::Substring;
    } else {
        m_error = "Invalid attribute matcher"_s;
        return std::nullopt;
    }

    skip_whitespace();

    // Value
    if (peek() == '"' || peek() == '\'') {
        unicode::CodePoint quote = consume();
        StringBuilder value;
        while (!at_end() && peek() != quote) {
            value.append(consume());
        }
        if (!at_end()) consume(); // closing quote
        sel.value = value.to_string();
    } else {
        sel.value = consume_ident();
    }

    skip_whitespace();

    // Case sensitivity flag
    if (peek() == 'i' || peek() == 'I') {
        consume();
        sel.case_insensitive = true;
        skip_whitespace();
    }

    if (peek() != ']') {
        m_error = "Expected ']' in attribute selector"_s;
        return std::nullopt;
    }
    consume();

    return sel;
}

std::optional<PseudoClassSelector> SelectorParser::parse_pseudo_class() {
    PseudoClassSelector sel;
    sel.name = consume_ident();

    if (sel.name.empty()) {
        m_error = "Expected pseudo-class name"_s;
        return std::nullopt;
    }

    // Functional pseudo-class
    if (peek() == '(') {
        consume();
        StringBuilder arg;
        int depth = 1;
        while (!at_end() && depth > 0) {
            if (peek() == '(') depth++;
            if (peek() == ')') depth--;
            if (depth > 0) {
                arg.append(consume());
            }
        }
        if (!at_end()) consume(); // )
        sel.argument = arg.to_string();
    }

    return sel;
}

std::optional<PseudoElementSelector> SelectorParser::parse_pseudo_element() {
    PseudoElementSelector sel;
    sel.name = consume_ident();

    if (sel.name.empty()) {
        m_error = "Expected pseudo-element name"_s;
        return std::nullopt;
    }

    return sel;
}

void SelectorParser::skip_whitespace() {
    while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
        consume();
    }
}

bool SelectorParser::at_end() const {
    return m_position >= m_input.length();
}

unicode::CodePoint SelectorParser::peek(usize offset) const {
    if (m_position + offset >= m_input.length()) return 0;
    return m_input[m_position + offset];
}

unicode::CodePoint SelectorParser::consume() {
    if (m_position >= m_input.length()) return 0;
    return m_input[m_position++];
}

bool SelectorParser::consume_if(unicode::CodePoint expected) {
    if (peek() == expected) {
        consume();
        return true;
    }
    return false;
}

String SelectorParser::consume_ident() {
    StringBuilder result;

    // Handle leading dash
    if (peek() == '-') {
        result.append(consume());
    }

    // First char must be letter, underscore, or non-ASCII
    unicode::CodePoint cp = peek();
    if (!std::isalpha(cp) && cp != '_' && cp <= 0x7F) {
        if (result.length() > 0) {
            // Just a dash, not valid
            m_position--;
            return String();
        }
        return String();
    }

    while (!at_end()) {
        cp = peek();
        if (std::isalnum(cp) || cp == '-' || cp == '_' || cp > 0x7F) {
            result.append(consume());
        } else if (cp == '\\') {
            // Escape sequence
            consume();
            if (!at_end()) {
                result.append(consume());
            }
        } else {
            break;
        }
    }

    return result.to_string();
}

// ============================================================================
// SelectorMatcher implementation
// ============================================================================

bool SelectorMatcher::matches(const SelectorList& selectors, const dom::Element& element) {
    for (const auto& sel : selectors.selectors) {
        if (matches(sel, element)) {
            return true;
        }
    }
    return false;
}

bool SelectorMatcher::matches(const ComplexSelector& selector, const dom::Element& element) {
    if (selector.parts.empty()) return false;

    // Start from the last part (subject)
    const dom::Element* current = &element;

    for (auto it = selector.parts.rbegin(); it != selector.parts.rend(); ++it) {
        if (!current) return false;

        if (!matches(it->compound, *current)) {
            return false;
        }

        // Move to next part based on combinator
        if (std::next(it) != selector.parts.rend()) {
            auto combinator = it->combinator;
            if (!combinator) {
                // This is the first part (rightmost), continue
                ++it;
                if (it == selector.parts.rend()) break;
                combinator = std::prev(it)->combinator;
            }

            switch (*combinator) {
                case Combinator::Descendant: {
                    // Any ancestor must match
                    const dom::Element* ancestor = current->parent_node() ?
                        current->parent_node()->as_element() : nullptr;
                    bool found = false;
                    while (ancestor) {
                        if (matches(std::next(it)->compound, *ancestor)) {
                            current = ancestor;
                            found = true;
                            break;
                        }
                        ancestor = ancestor->parent_node() ?
                            ancestor->parent_node()->as_element() : nullptr;
                    }
                    if (!found) return false;
                    break;
                }
                case Combinator::Child: {
                    // Direct parent must match
                    if (!current->parent_node() || !current->parent_node()->is_element()) {
                        return false;
                    }
                    current = current->parent_node()->as_element();
                    break;
                }
                case Combinator::NextSibling: {
                    // Previous sibling must match
                    current = current->previous_element_sibling();
                    if (!current) return false;
                    break;
                }
                case Combinator::SubsequentSibling: {
                    // Any previous sibling must match
                    const dom::Element* sibling = current->previous_element_sibling();
                    bool found = false;
                    while (sibling) {
                        if (matches(std::next(it)->compound, *sibling)) {
                            current = sibling;
                            found = true;
                            break;
                        }
                        sibling = sibling->previous_element_sibling();
                    }
                    if (!found) return false;
                    break;
                }
            }
        }
    }

    return true;
}

bool SelectorMatcher::matches(const CompoundSelector& selector, const dom::Element& element) {
    for (const auto& simple : selector.selectors) {
        if (!matches(simple, element)) {
            return false;
        }
    }
    return true;
}

bool SelectorMatcher::matches(const SimpleSelector& selector, const dom::Element& element) {
    return std::visit([&element](const auto& sel) -> bool {
        using T = std::decay_t<decltype(sel)>;

        if constexpr (std::is_same_v<T, UniversalSelector>) {
            return true;
        } else if constexpr (std::is_same_v<T, TypeSelector>) {
            return element.local_name() == sel.tag_name.to_lowercase();
        } else if constexpr (std::is_same_v<T, IdSelector>) {
            return element.id() == sel.id;
        } else if constexpr (std::is_same_v<T, ClassSelector>) {
            auto classes = element.class_list();
            return std::find(classes.begin(), classes.end(), sel.class_name) != classes.end();
        } else if constexpr (std::is_same_v<T, AttributeSelector>) {
            auto attr = element.get_attribute(sel.attribute);
            if (!attr) {
                return sel.matcher == AttributeSelector::Matcher::Exists ? false : false;
            }
            if (sel.matcher == AttributeSelector::Matcher::Exists) {
                return true;
            }

            String value = *attr;
            String match_value = sel.value;
            if (sel.case_insensitive) {
                value = value.to_lowercase();
                match_value = match_value.to_lowercase();
            }

            switch (sel.matcher) {
                case AttributeSelector::Matcher::Exists:
                    return true;
                case AttributeSelector::Matcher::Equals:
                    return value == match_value;
                case AttributeSelector::Matcher::Includes: {
                    // Space-separated list contains value
                    usize start = 0;
                    for (usize i = 0; i <= value.length(); ++i) {
                        if (i == value.length() || value[i] == ' ') {
                            if (i > start && value.substring(start, i - start) == match_value) {
                                return true;
                            }
                            start = i + 1;
                        }
                    }
                    return false;
                }
                case AttributeSelector::Matcher::DashMatch:
                    return value == match_value ||
                           (value.starts_with(match_value) &&
                            value.length() > match_value.length() &&
                            value[match_value.length()] == '-');
                case AttributeSelector::Matcher::Prefix:
                    return value.starts_with(match_value);
                case AttributeSelector::Matcher::Suffix:
                    return value.ends_with(match_value);
                case AttributeSelector::Matcher::Substring:
                    return value.find(match_value) != String::npos;
            }
            return false;
        } else if constexpr (std::is_same_v<T, PseudoClassSelector>) {
            // Handle common pseudo-classes
            String name = sel.name.to_lowercase();
            if (name == "first-child"_s) {
                return element.previous_element_sibling() == nullptr;
            }
            if (name == "last-child"_s) {
                return element.next_element_sibling() == nullptr;
            }
            if (name == "only-child"_s) {
                return element.previous_element_sibling() == nullptr &&
                       element.next_element_sibling() == nullptr;
            }
            if (name == "empty"_s) {
                return !element.has_children();
            }
            if (name == "root"_s) {
                return element.parent_node() &&
                       element.parent_node()->node_type() == dom::NodeType::Document;
            }
            // TODO: Implement more pseudo-classes
            return false;
        } else if constexpr (std::is_same_v<T, PseudoElementSelector>) {
            // Pseudo-elements don't match elements directly
            return false;
        }

        return false;
    }, selector);
}

dom::Element* SelectorMatcher::query_selector(
    const SelectorList& selectors, const dom::Element& root)
{
    std::function<dom::Element*(const dom::Element&)> find = [&](const dom::Element& elem) -> dom::Element* {
        for (const auto& child : elem.child_nodes()) {
            if (child->is_element()) {
                auto* el = child->as_element();
                if (matches(selectors, *el)) {
                    return el;
                }
                if (auto* found = find(*el)) {
                    return found;
                }
            }
        }
        return nullptr;
    };

    return find(root);
}

std::vector<dom::Element*> SelectorMatcher::query_selector_all(
    const SelectorList& selectors, const dom::Element& root)
{
    std::vector<dom::Element*> results;

    std::function<void(const dom::Element&)> collect = [&](const dom::Element& elem) {
        for (const auto& child : elem.child_nodes()) {
            if (child->is_element()) {
                auto* el = child->as_element();
                if (matches(selectors, *el)) {
                    results.push_back(el);
                }
                collect(*el);
            }
        }
    };

    collect(root);
    return results;
}

} // namespace lithium::css
