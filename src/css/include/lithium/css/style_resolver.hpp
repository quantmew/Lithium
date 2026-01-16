#pragma once

#include "parser.hpp"
#include "selector.hpp"
#include "value.hpp"
#include "lithium/dom/document.hpp"
#include <unordered_map>

namespace lithium::css {

// ============================================================================
// Cascade Origin
// ============================================================================

enum class CascadeOrigin {
    UserAgent,
    User,
    Author
};

// ============================================================================
// Matched Rule
// ============================================================================

struct MatchedRule {
    const StyleRule* rule;
    Specificity specificity;
    CascadeOrigin origin;
    u32 source_order;
};

// ============================================================================
// Style Resolver - Computes styles for DOM elements
// ============================================================================

class StyleResolver {
public:
    StyleResolver();

    // Add stylesheets
    void add_stylesheet(const Stylesheet& stylesheet, CascadeOrigin origin = CascadeOrigin::Author);
    void add_user_agent_stylesheet(const Stylesheet& stylesheet);
    void add_user_stylesheet(const Stylesheet& stylesheet);

    // Clear all stylesheets
    void clear_stylesheets();

    // Resolve styles for an element
    [[nodiscard]] ComputedValue resolve(const dom::Element& element) const;

    // Resolve styles for entire document
    void resolve_document(dom::Document& document);

    // Get computed style for an element (cached)
    [[nodiscard]] const ComputedValue* get_computed_style(const dom::Element& element) const;

    // Invalidate cached styles
    void invalidate_element(const dom::Element& element);
    void invalidate_all();

private:
    // Find all matching rules for an element
    [[nodiscard]] std::vector<MatchedRule> collect_matching_rules(const dom::Element& element) const;

    // Sort rules by cascade order
    static void sort_by_cascade(std::vector<MatchedRule>& rules);

    // Apply matched rules to compute style
    [[nodiscard]] ComputedValue apply_cascade(
        const std::vector<MatchedRule>& rules,
        const dom::Element& element) const;

    // Apply inheritance
    void apply_inheritance(ComputedValue& style, const ComputedValue* parent_style) const;

    // Apply initial values
    static void apply_initial_values(ComputedValue& style);

    // Property inheritance check
    [[nodiscard]] static bool is_inherited_property(const String& property);

    // Default style (user-agent stylesheet)
    [[nodiscard]] static ComputedValue default_style_for_element(const dom::Element& element);

    // Style storage
    struct StylesheetEntry {
        Stylesheet stylesheet;
        CascadeOrigin origin;
    };
    std::vector<StylesheetEntry> m_stylesheets;

    // Style cache
    mutable std::unordered_map<const dom::Element*, ComputedValue> m_style_cache;
};

// ============================================================================
// Default User-Agent Stylesheet
// ============================================================================

[[nodiscard]] const Stylesheet& default_user_agent_stylesheet();

} // namespace lithium::css
