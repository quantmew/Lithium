/**
 * CSS Style Resolver - Computes styles for DOM elements
 */

#include "lithium/css/style_resolver.hpp"
#include "lithium/dom/element.hpp"
#include <algorithm>
#include <unordered_set>

namespace lithium::css {

// ============================================================================
// StyleResolver implementation
// ============================================================================

StyleResolver::StyleResolver() {
    // Add default user-agent stylesheet
    add_user_agent_stylesheet(default_user_agent_stylesheet());
}

void StyleResolver::add_stylesheet(const Stylesheet& stylesheet, CascadeOrigin origin) {
    m_stylesheets.push_back({stylesheet, origin});
    invalidate_all();
}

void StyleResolver::add_user_agent_stylesheet(const Stylesheet& stylesheet) {
    add_stylesheet(stylesheet, CascadeOrigin::UserAgent);
}

void StyleResolver::add_user_stylesheet(const Stylesheet& stylesheet) {
    add_stylesheet(stylesheet, CascadeOrigin::User);
}

void StyleResolver::clear_stylesheets() {
    m_stylesheets.clear();
    invalidate_all();
    // Re-add user-agent stylesheet
    add_user_agent_stylesheet(default_user_agent_stylesheet());
}

ComputedValue StyleResolver::resolve(const dom::Element& element) const {
    // Check cache first
    auto cached = get_computed_style(element);
    if (cached) {
        return *cached;
    }

    // Collect matching rules
    auto rules = collect_matching_rules(element);

    // Sort by cascade order
    sort_by_cascade(rules);

    // Apply cascade
    auto style = apply_cascade(rules, element);

    // Cache result
    m_style_cache[&element] = style;

    return style;
}

void StyleResolver::resolve_document(dom::Document& document) {
    auto* root = document.document_element();
    if (!root) return;

    // Simple recursive resolution
    std::function<void(dom::Element&)> resolve_recursive = [&](dom::Element& elem) {
        (void)resolve(elem);
        for (auto* child = elem.first_child(); child; child = child->next_sibling()) {
            if (child->is_element()) {
                resolve_recursive(*child->as_element());
            }
        }
    };

    resolve_recursive(*root);
}

const ComputedValue* StyleResolver::get_computed_style(const dom::Element& element) const {
    auto it = m_style_cache.find(&element);
    if (it != m_style_cache.end()) {
        return &it->second;
    }
    return nullptr;
}

void StyleResolver::invalidate_element(const dom::Element& element) {
    m_style_cache.erase(&element);
}

void StyleResolver::invalidate_all() {
    m_style_cache.clear();
}

std::vector<MatchedRule> StyleResolver::collect_matching_rules(const dom::Element& element) const {
    std::vector<MatchedRule> matches;
    u32 source_order = 0;

    for (const auto& entry : m_stylesheets) {
        for (const auto& rule : entry.stylesheet.rules) {
            if (rule.is<StyleRule>()) {
                const auto& style_rule = rule.get<StyleRule>();

                // Check if any selector matches
                for (const auto& selector : style_rule.selectors.selectors) {
                    if (selector.matches(element)) {
                        MatchedRule matched;
                        matched.rule = &style_rule;
                        matched.origin = entry.origin;
                        matched.specificity = calculate_specificity(selector);
                        matched.source_order = source_order++;
                        matches.push_back(matched);
                        break; // Only add once per rule
                    }
                }
            }
        }
    }

    return matches;
}

void StyleResolver::sort_by_cascade(std::vector<MatchedRule>& rules) {
    std::stable_sort(rules.begin(), rules.end(), [](const MatchedRule& a, const MatchedRule& b) {
        // First sort by origin
        if (static_cast<int>(a.origin) != static_cast<int>(b.origin)) {
            return static_cast<int>(a.origin) < static_cast<int>(b.origin);
        }

        // Then by specificity
        if (a.specificity.a != b.specificity.a) return a.specificity.a < b.specificity.a;
        if (a.specificity.b != b.specificity.b) return a.specificity.b < b.specificity.b;
        if (a.specificity.c != b.specificity.c) return a.specificity.c < b.specificity.c;

        // Then by source order
        return a.source_order < b.source_order;
    });
}

ComputedValue StyleResolver::apply_cascade(
    const std::vector<MatchedRule>& rules,
    const dom::Element& element) const
{
    // Start with default style
    ComputedValue style = default_style_for_element(element);

    // Apply matched rules in order
    for (const auto& matched : rules) {
        for (const auto& decl : matched.rule->declarations.declarations) {
            // Apply declaration to style
            // Simplified: just use ValueParser
            StringBuilder sb;
            for (const auto& cv : decl.value) {
                if (auto* preserved = std::get_if<PreservedToken>(&cv)) {
                    sb.append(preserved->value);
                }
            }
            (void)ValueParser::apply_property(style, decl.property, sb.build());
        }
    }

    // Apply inheritance from parent
    auto* parent = element.parent_node();
    if (parent && parent->is_element()) {
        auto parent_style = get_computed_style(*parent->as_element());
        apply_inheritance(style, parent_style);
    }

    return style;
}

void StyleResolver::apply_inheritance(ComputedValue& style, const ComputedValue* parent_style) const {
    if (!parent_style) return;

    // Inherit properties that haven't been explicitly set
    // In a full implementation, we'd track which properties were set

    // Color is inherited
    if (style.color.r == 0 && style.color.g == 0 && style.color.b == 0 && style.color.a == 255) {
        style.color = parent_style->color;
    }

    // Font properties are inherited
    if (style.font_family.empty()) {
        style.font_family = parent_style->font_family;
    }
}

void StyleResolver::apply_initial_values(ComputedValue& style) {
    // Set initial values for properties
    style.display = Display::Inline;
    style.position = Position::Static;
    style.color = Color{0, 0, 0, 255};
    style.background_color = Color{0, 0, 0, 0}; // transparent
    style.font_size = Length{16, LengthUnit::Px};
    style.font_weight = FontWeight::Normal;
    style.font_style = FontStyle::Normal;
}

bool StyleResolver::is_inherited_property(const String& property) {
    static const std::unordered_set<std::string> inherited = {
        "color", "font", "font-family", "font-size", "font-style",
        "font-weight", "line-height", "text-align", "visibility",
        "cursor", "direction"
    };
    return inherited.count(std::string(property.c_str())) > 0;
}

ComputedValue StyleResolver::default_style_for_element(const dom::Element& element) {
    ComputedValue style;
    apply_initial_values(style);

    String tag = element.local_name().to_lowercase();

    // Default display
    static const std::unordered_set<std::string> block_elements = {
        "div", "p", "h1", "h2", "h3", "h4", "h5", "h6",
        "ul", "ol", "li", "table", "form", "header", "footer",
        "main", "section", "article", "nav", "aside", "pre"
    };

    if (block_elements.count(std::string(tag.c_str())) > 0) {
        style.display = Display::Block;
    }

    // Default font size for headings
    if (tag == "h1"_s) {
        style.font_size = Length{32, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "h2"_s) {
        style.font_size = Length{24, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "h3"_s) {
        style.font_size = Length{18.72f, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "h4"_s) {
        style.font_size = Length{16, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "h5"_s) {
        style.font_size = Length{13.28f, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "h6"_s) {
        style.font_size = Length{10.72f, LengthUnit::Px};
        style.font_weight = FontWeight::Bold;
    } else if (tag == "b"_s || tag == "strong"_s) {
        style.font_weight = FontWeight::Bold;
    } else if (tag == "i"_s || tag == "em"_s) {
        style.font_style = FontStyle::Italic;
    }

    return style;
}

// ============================================================================
// User-agent stylesheet
// ============================================================================

const Stylesheet& default_user_agent_stylesheet() {
    static Stylesheet ua_stylesheet;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        // For now, just use an empty stylesheet
        // Full UA stylesheet would be parsed from CSS
    }

    return ua_stylesheet;
}

} // namespace lithium::css
