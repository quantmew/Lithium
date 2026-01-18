/**
 * CSS Style Resolver - Computes styles for DOM elements
 */

#include "lithium/css/style_resolver.hpp"
#include "lithium/dom/element.hpp"
#include <algorithm>
#include <unordered_set>
#include <iostream>

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
        std::cout << "  resolve: returning CACHED style for '" << element.tag_name().c_str() << "'" << std::endl;
        return *cached;
    }

    std::cout << "  resolve: COMPUTING style for '" << element.tag_name().c_str() << "'" << std::endl;

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
    std::cout << "  apply_cascade for '" << element.tag_name().c_str()
              << "' with " << rules.size() << " matched rules" << std::endl;

    // Step 1: Apply inheritance from parent FIRST (before any rules)
    ComputedValue style;
    apply_initial_values(style);

    auto* parent = element.parent_node();
    if (parent && parent->is_element()) {
        auto parent_style = get_computed_style(*parent->as_element());
        apply_inheritance(style, parent_style);
        std::cout << "    Applied inheritance from parent" << std::endl;
    }

    // Step 2: Apply element-specific defaults (like display for headings)
    // This allows UA rules to override these
    String tag = element.local_name().to_lowercase();

    // Default display (can be overridden by UA stylesheet)
    static const std::unordered_set<std::string> block_elements = {
        "div", "p", "h1", "h2", "h3", "h4", "h5", "h6",
        "ul", "ol", "li", "table", "form", "header", "footer",
        "main", "section", "article", "nav", "aside", "pre"
    };

    if (block_elements.count(std::string(tag.c_str())) > 0) {
        style.display = Display::Block;
    }

    // Step 3: Apply matched rules (UA, user, author) in cascade order
    for (const auto& matched : rules) {
        std::cout << "    Applying rule with specificity: "
                  << matched.specificity.a << "." << matched.specificity.b << "." << matched.specificity.c
                  << " origin=" << static_cast<int>(matched.origin) << std::endl;
        for (const auto& decl : matched.rule->declarations.declarations) {
            // Build value string
            StringBuilder sb;
            for (const auto& cv : decl.value) {
                if (auto* preserved = std::get_if<PreservedToken>(&cv)) {
                    sb.append(preserved->value);
                }
            }
            String value_str = sb.build();
            std::cout << "      Declaration: " << decl.property.c_str() << "=" << value_str.c_str() << std::endl;
            // Apply declaration to style (this overrides inherited values)
            (void)ValueParser::apply_property(style, decl.property, value_str);
        }
    }

    return style;
}

void StyleResolver::apply_inheritance(ComputedValue& style, const ComputedValue* parent_style) const {
    if (!parent_style) return;

    // CSS inherited properties should always inherit unless explicitly set
    // For now, we'll always inherit these properties before applying rules
    // The rules will override if explicitly set

    // Font properties are inherited
    style.font_family = parent_style->font_family;
    style.font_size = parent_style->font_size;
    style.font_weight = parent_style->font_weight;
    style.font_style = parent_style->font_style;
    style.line_height = parent_style->line_height;

    // Color is inherited
    style.color = parent_style->color;

    // Text alignment is inherited
    style.text_align = parent_style->text_align;
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
// Based on W3C CSS 2.1 default style sheet:
// https://www.w3.org/TR/CSS2/sample.html
// And modern browser implementations (Chrome, Firefox, Safari)
// ============================================================================

const Stylesheet& default_user_agent_stylesheet() {
    static Stylesheet ua_stylesheet;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        // Parse CSS from string
        Parser parser;
        String ua_css = R"(
/* Block-level elements */
html, body, div, h1, h2, h3, h4, h5, h6, p, ul, ol, li,
dl, dt, dd, table, caption, tbody, thead, tfoot, tr, th, td,
article, aside, footer, header, hgroup, main, nav, section {
    display: block;
}

/* Head elements - not displayed */
head, style, script, title, meta, link {
    display: none;
}

/* Lists */
ul, ol {
    list-style-type: disc;
    padding-left: 40px;
}

ol {
    list-style-type: decimal;
}

li {
    display: list-item;
}

/* Tables */
table {
    display: table;
    border-collapse: separate;
    border-spacing: 2px;
    border-color: gray;
}

td, th {
    display: table-cell;
    padding: 1px;
}

thead {
    display: table-header-group;
}

tbody {
    display: table-row-group;
}

tfoot {
    display: table-footer-group;
}

tr {
    display: table-row;
}

/* Inline elements */
b, strong {
    font-weight: bold;
}

i, em {
    font-style: italic;
}

a {
    color: blue;
    text-decoration: underline;
}

/* Headers */
h1 {
    font-size: 2em;
    font-weight: bold;
    margin-top: 0.67em;
    margin-bottom: 0.67em;
}

h2 {
    font-size: 1.5em;
    font-weight: bold;
    margin-top: 0.75em;
    margin-bottom: 0.75em;
}

h3 {
    font-size: 1.17em;
    font-weight: bold;
    margin-top: 0.83em;
    margin-bottom: 0.83em;
}

h4 {
    font-size: 1em;
    font-weight: bold;
    margin-top: 1em;
    margin-bottom: 1em;
}

h5 {
    font-size: 0.83em;
    font-weight: bold;
    margin-top: 1.17em;
    margin-bottom: 1.17em;
}

h6 {
    font-size: 0.67em;
    font-weight: bold;
    margin-top: 1.33em;
    margin-bottom: 1.33em;
}

/* Paragraphs and blocks */
p {
    margin-top: 1em;
    margin-bottom: 1em;
}

ul, ol {
    margin-top: 1em;
    margin-bottom: 1em;
}

/* Code elements */
code, pre {
    font-family: monospace;
}

/* Quotes */
blockquote {
    margin: 1em 40px;
}

/* Horizontal rule */
hr {
    border: 1px inset;
    margin: 0.5em auto;
}

/* Images and replaced elements */
img, input, textarea, select, button {
    display: inline-block;
}

/* Form elements */
input, textarea, select, button {
    font: inherit;
}

/* Hidden elements */
[hidden] {
    display: none;
}
)";
        ua_stylesheet = parser.parse_stylesheet(ua_css);

        std::cout << "User Agent Stylesheet loaded: "
                  << ua_stylesheet.style_rules().size() << " rules" << std::endl;
    }

    return ua_stylesheet;
}

} // namespace lithium::css
