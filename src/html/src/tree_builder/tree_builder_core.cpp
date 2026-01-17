/**
 * HTML Tree Builder core (shared helpers and glue)
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>
#include <unordered_set>

namespace lithium::html {

bool is_mathml_text_integration_point(const String& name_lower);

TreeBuilder::TreeBuilder() = default;

bool TreeBuilder::in_foreign_content() const {
    static const String SVG_NS = "http://www.w3.org/2000/svg"_s;
    static const String MATHML_NS = "http://www.w3.org/1998/Math/MathML"_s;

    auto* adjusted = adjusted_current_node();
    if (!adjusted) return false;

    auto ns = adjusted->namespace_uri();
    if (ns.empty()) return false;

    auto local = adjusted->local_name().to_lowercase();
    if (ns == MATHML_NS) {
        if (is_mathml_text_integration_point(local)) {
            return false;
        }
        if (local == "annotation-xml"_s) {
            auto encoding = adjusted->get_attribute("encoding"_s).value_or(String()).to_lowercase();
            if (encoding == "text/html"_s || encoding == "application/xhtml+xml"_s) {
                return false;
            }
        }
    }

    if (ns == SVG_NS) {
        if (local == "foreignobject"_s || local == "desc"_s || local == "title"_s) {
            return false;
        }
    }

    return true;
}

String svg_camel_case(const String& name_lower) {
    struct Entry { const char* from; const char* to; };
    static const Entry mappings[] = {
        {"altglyph", "altGlyph"}, {"altglyphdef", "altGlyphDef"}, {"altglyphitem", "altGlyphItem"},
        {"animatecolor", "animateColor"}, {"animatemotion", "animateMotion"}, {"animatetransform", "animateTransform"},
        {"clippath", "clipPath"}, {"feblend", "feBlend"}, {"fecolormatrix", "feColorMatrix"},
        {"fecomponenttransfer", "feComponentTransfer"}, {"fecomposite", "feComposite"},
        {"feconvolvematrix", "feConvolveMatrix"}, {"fediffuselighting", "feDiffuseLighting"},
        {"fedisplacementmap", "feDisplacementMap"}, {"fedistantlight", "feDistantLight"},
        {"fedropshadow", "feDropShadow"}, {"feflood", "feFlood"}, {"fefunca", "feFuncA"},
        {"fefuncb", "feFuncB"}, {"fefuncg", "feFuncG"}, {"fefuncr", "feFuncR"},
        {"fegaussianblur", "feGaussianBlur"}, {"feimage", "feImage"}, {"femerge", "feMerge"},
        {"femergenode", "feMergeNode"}, {"femorphology", "feMorphology"}, {"feoffset", "feOffset"},
        {"fepointlight", "fePointLight"}, {"fespecularlighting", "feSpecularLighting"},
        {"fespotlight", "feSpotLight"}, {"fetile", "feTile"}, {"feturbulence", "feTurbulence"},
        {"foreignobject", "foreignObject"}, {"glyphref", "glyphRef"}, {"lineargradient", "linearGradient"},
        {"radialgradient", "radialGradient"}, {"textpath", "textPath"}
    };
    for (const auto& m : mappings) {
        if (name_lower == String(m.from)) {
            return String(m.to);
        }
    }
    return name_lower;
}

String svg_attribute_camel_case(const String& name_lower) {
    struct Entry { const char* from; const char* to; };
    static const Entry mappings[] = {
        {"attributename", "attributeName"}, {"attributetype", "attributeType"},
        {"basefrequency", "baseFrequency"}, {"clippathunits", "clipPathUnits"},
        {"diffuseconstant", "diffuseConstant"}, {"edgemode", "edgeMode"},
        {"filterunits", "filterUnits"}, {"glyphref", "glyphRef"},
        {"gradienttransform", "gradientTransform"}, {"gradientunits", "gradientUnits"},
        {"kernelmatrix", "kernelMatrix"}, {"kernelunitlength", "kernelUnitLength"},
        {"keypoints", "keyPoints"}, {"keysplines", "keySplines"}, {"keytimes", "keyTimes"},
        {"lengthadjust", "lengthAdjust"}, {"limitingconeangle", "limitingConeAngle"},
        {"markerheight", "markerHeight"}, {"markerunits", "markerUnits"}, {"markerwidth", "markerWidth"},
        {"maskcontentunits", "maskContentUnits"}, {"maskunits", "maskUnits"},
        {"numoctaves", "numOctaves"}, {"pathlength", "pathLength"},
        {"patterncontentunits", "patternContentUnits"}, {"patterntransform", "patternTransform"},
        {"patternunits", "patternUnits"}, {"pointsatx", "pointsAtX"},
        {"pointsaty", "pointsAtY"}, {"pointsatz", "pointsAtZ"},
        {"preservealpha", "preserveAlpha"}, {"preserveaspectratio", "preserveAspectRatio"},
        {"primitiveunits", "primitiveUnits"}, {"refx", "refX"}, {"refy", "refY"},
        {"repeatcount", "repeatCount"}, {"repeatdur", "repeatDur"},
        {"requiredextensions", "requiredExtensions"}, {"requiredfeatures", "requiredFeatures"},
        {"specularconstant", "specularConstant"}, {"specularexponent", "specularExponent"},
        {"spreadmethod", "spreadMethod"}, {"startoffset", "startOffset"},
        {"stddeviation", "stdDeviation"}, {"stitchtiles", "stitchTiles"},
        {"surfacescale", "surfaceScale"}, {"systemlanguage", "systemLanguage"},
        {"tablevalues", "tableValues"}, {"targetx", "targetX"}, {"targety", "targetY"},
        {"textlength", "textLength"}, {"viewbox", "viewBox"}, {"viewtarget", "viewTarget"},
        {"xchannelselector", "xChannelSelector"}, {"ychannelselector", "yChannelSelector"},
        {"zoomandpan", "zoomAndPan"}
    };
    for (const auto& m : mappings) {
        if (name_lower == String(m.from)) {
            return String(m.to);
        }
    }
    return name_lower;
}

bool is_mathml_text_integration_point(const String& name_lower) {
    static const std::array<const char*, 5> POINTS = {"mi", "mo", "mn", "ms", "mtext"};
    for (const auto* n : POINTS) {
        if (name_lower == String(n)) {
            return true;
        }
    }
    return false;
}

void TreeBuilder::set_document(RefPtr<dom::Document> document) {
    m_document = document;
}

void TreeBuilder::prepare_for_fragment(RefPtr<dom::Element> context_element) {
    m_open_elements.clear();
    m_active_formatting_elements.clear();
    std::stack<InsertionMode> empty_modes;
    m_template_insertion_modes.swap(empty_modes);

    m_context_element = context_element.get();
    m_head_element = nullptr;
    m_form_element = nullptr;
    m_frameset_ok = true;
    m_parser_cannot_change_mode = true;
    m_is_iframe_srcdoc = false;

    if (context_element) {
        if (context_element->local_name() == "form"_s) {
            m_form_element = context_element.get();
        }
        m_open_elements.push_back(std::move(context_element));
        reset_insertion_mode_appropriately();
    } else {
        m_insertion_mode = InsertionMode::Initial;
    }
}

void TreeBuilder::process_token(const Token& token) {
    bool check_self_closing = false;
    if (auto* tag = std::get_if<TagToken>(&token)) {
        if (!tag->is_end_tag && tag->self_closing) {
            m_self_closing_flag_acknowledged = false;
            check_self_closing = true;
        } else {
            m_self_closing_flag_acknowledged = true;
        }
    } else {
        m_self_closing_flag_acknowledged = true;
    }

    if (process_foreign_content(token)) {
        if (check_self_closing && !m_self_closing_flag_acknowledged) {
            parse_error("Unacknowledged self-closing flag"_s);
            m_self_closing_flag_acknowledged = true;
        }
        return;
    }

    switch (m_insertion_mode) {
        case InsertionMode::Initial:
            process_initial(token);
            break;
        case InsertionMode::BeforeHtml:
            process_before_html(token);
            break;
        case InsertionMode::BeforeHead:
            process_before_head(token);
            break;
        case InsertionMode::InHead:
            process_in_head(token);
            break;
        case InsertionMode::InHeadNoscript:
            process_in_head_noscript(token);
            break;
        case InsertionMode::AfterHead:
            process_after_head(token);
            break;
        case InsertionMode::InBody:
            process_in_body(token);
            break;
        case InsertionMode::Text:
            process_text(token);
            break;
        case InsertionMode::InTable:
            process_in_table(token);
            break;
        case InsertionMode::InTableText:
            process_in_table_text(token);
            break;
        case InsertionMode::InCaption:
            process_in_caption(token);
            break;
        case InsertionMode::InColumnGroup:
            process_in_column_group(token);
            break;
        case InsertionMode::InTableBody:
            process_in_table_body(token);
            break;
        case InsertionMode::InRow:
            process_in_row(token);
            break;
        case InsertionMode::InCell:
            process_in_cell(token);
            break;
        case InsertionMode::InSelect:
            process_in_select(token);
            break;
        case InsertionMode::InSelectInTable:
            process_in_select_in_table(token);
            break;
        case InsertionMode::InTemplate:
            process_in_template(token);
            break;
        case InsertionMode::AfterBody:
            process_after_body(token);
            break;
        case InsertionMode::InFrameset:
            process_in_frameset(token);
            break;
        case InsertionMode::AfterFrameset:
            process_after_frameset(token);
            break;
        case InsertionMode::AfterAfterBody:
            process_after_after_body(token);
            break;
        case InsertionMode::AfterAfterFrameset:
            process_after_after_frameset(token);
            break;
        default:
            process_in_body(token);
            break;
    }

    if (check_self_closing && !m_self_closing_flag_acknowledged) {
        String name;
        if (auto* tag = std::get_if<TagToken>(&token)) {
            name = tag->name;
        }
        parse_error("non-void-self-closing"_s + (name.empty() ? String() : String(":"_s + name)));
        m_self_closing_flag_acknowledged = true;
    }
}

void TreeBuilder::process_using_rules_for(InsertionMode mode, const Token& token) {
    auto saved_mode = m_insertion_mode;
    m_insertion_mode = mode;
    process_token(token);
    m_insertion_mode = saved_mode;
}

RefPtr<dom::Element> TreeBuilder::create_element(const TagToken& token, const String& namespace_uri) {
    RefPtr<dom::Element> element;
    if (!namespace_uri.empty()) {
        element = m_document->create_element_ns(namespace_uri, token.name);
    } else {
        element = m_document->create_element(token.name);
    }
    return element;
}

RefPtr<dom::Element> TreeBuilder::create_element_for_token(const TagToken& token) {
    static const String SVG_NS = "http://www.w3.org/2000/svg"_s;
    static const String MATHML_NS = "http://www.w3.org/1998/Math/MathML"_s;

    auto adjusted = adjusted_current_node();
    auto context_ns = adjusted ? adjusted->namespace_uri() : String();
    auto context_name = adjusted ? adjusted->local_name() : String();
    auto context_name_lower = context_name.to_lowercase();
    auto name_lower = token.name.to_lowercase();

    String ns;
    String adjusted_name = token.name;

    if (name_lower == "svg"_s) {
        ns = SVG_NS;
        adjusted_name = svg_camel_case(name_lower);
    } else if (name_lower == "math"_s) {
        ns = MATHML_NS;
        adjusted_name = name_lower;
    } else if (context_ns == SVG_NS) {
        if (context_name_lower == "foreignobject"_s || context_name_lower == "desc"_s || context_name_lower == "title"_s) {
            ns = String(); // HTML namespace
        } else {
            ns = SVG_NS;
            adjusted_name = svg_camel_case(name_lower);
        }
    } else if (context_ns == MATHML_NS) {
        if (context_name_lower == "annotation-xml"_s) {
            auto encoding = adjusted ? adjusted->get_attribute("encoding"_s).value_or(String()).to_lowercase() : String();
            if (encoding == "text/html"_s || encoding == "application/xhtml+xml"_s) {
                ns = String();
            } else {
                ns = MATHML_NS;
            }
        } else if (is_mathml_text_integration_point(context_name_lower) &&
                   name_lower != "mglyph"_s && name_lower != "malignmark"_s) {
            ns = String();
        } else {
            ns = MATHML_NS;
        }
    }

    TagToken adjusted_token = token;
    adjusted_token.name = adjusted_name;

    auto element = create_element(adjusted_token, ns);

    // Attribute adjustments per spec with namespace tracking
    struct AdjustedAttribute {
        String name;
        String value;
        String namespace_uri;
        String local_name;
    };

    static const String XLINK_NS = "http://www.w3.org/1999/xlink"_s;
    static const String XML_NS = "http://www.w3.org/XML/1998/namespace"_s;
    static const String XMLNS_NS = "http://www.w3.org/2000/xmlns/"_s;

    std::vector<AdjustedAttribute> adjusted_attributes;
    adjusted_attributes.reserve(token.attributes.size());

    for (const auto& [attr_name, attr_value] : token.attributes) {
        auto name_lower_attr = attr_name.to_lowercase();
        AdjustedAttribute adjusted_attr;
        adjusted_attr.value = attr_value;
        adjusted_attr.name = attr_name;
        adjusted_attr.local_name = name_lower_attr;

        if (element->namespace_uri() == SVG_NS) {
            if (name_lower_attr.starts_with("xlink:"_s)) {
                adjusted_attr.namespace_uri = XLINK_NS;
                adjusted_attr.local_name = name_lower_attr.substring(6);
                adjusted_attr.name = "xlink:"_s + adjusted_attr.local_name;
            } else if (name_lower_attr.starts_with("xml:"_s)) {
                adjusted_attr.namespace_uri = XML_NS;
                adjusted_attr.local_name = name_lower_attr.substring(4);
                adjusted_attr.name = "xml:"_s + adjusted_attr.local_name;
            } else if (name_lower_attr.starts_with("xmlns:"_s)) {
                adjusted_attr.namespace_uri = XMLNS_NS;
                adjusted_attr.local_name = name_lower_attr.substring(6);
                adjusted_attr.name = "xmlns:"_s + adjusted_attr.local_name;
            } else if (name_lower_attr == "xmlns"_s) {
                adjusted_attr.namespace_uri = XMLNS_NS;
                adjusted_attr.local_name = "xmlns"_s;
                adjusted_attr.name = "xmlns"_s;
            } else {
                adjusted_attr.name = svg_attribute_camel_case(name_lower_attr);
                adjusted_attr.local_name = adjusted_attr.name.to_lowercase();
            }
        } else if (element->namespace_uri() == MATHML_NS) {
            if (name_lower_attr == "definitionurl"_s) {
                adjusted_attr.name = "definitionURL"_s;
                adjusted_attr.local_name = "definitionurl"_s;
            } else {
                adjusted_attr.name = name_lower_attr;
                adjusted_attr.local_name = name_lower_attr;
            }
        }

        adjusted_attributes.push_back(std::move(adjusted_attr));
    }

    std::unordered_set<String> seen_attributes;
    for (const auto& attr : adjusted_attributes) {
        String key = attr.namespace_uri.empty()
            ? attr.local_name
            : attr.namespace_uri + ":"_s + attr.local_name;
        if (seen_attributes.contains(key)) {
            parse_error("duplicate-attribute"_s);
            continue;
        }
        seen_attributes.insert(key);

        if (!attr.namespace_uri.empty()) {
            element->set_attribute_ns(attr.namespace_uri, attr.name, attr.value);
        } else {
            element->set_attribute(attr.name, attr.value);
        }
    }

    associate_form_owner(element.get(), adjusted_token);
    return element;
}

void TreeBuilder::insert_element(RefPtr<dom::Element> element) {
    auto insertion = appropriate_insertion_place();
    if (insertion.parent) {
        if (insertion.insert_before) {
            insertion.parent->insert_before(element, insertion.insert_before);
        } else {
            insertion.parent->append_child(element);
        }
    }
    push_open_element(element);
}

void TreeBuilder::insert_character(unicode::CodePoint cp) {
    auto insertion = appropriate_insertion_place();
    auto* insert_parent = insertion.parent;
    if (!insert_parent) return;

    dom::Node* adjacent = insertion.insert_before
        ? insertion.insert_before->previous_sibling()
        : insert_parent->last_child();

    String as_string = String::from_code_point(cp);
    if (adjacent && adjacent->is_text()) {
        adjacent->as_text()->append_data(as_string);
        return;
    }

    auto text = m_document->create_text_node(as_string);
    if (insertion.insert_before) {
        insert_parent->insert_before(text, insertion.insert_before);
    } else {
        insert_parent->append_child(text);
    }
}

void TreeBuilder::insert_comment(const CommentToken& token, dom::Node* position) {
    auto comment = m_document->create_comment(token.data);
    if (position) {
        position->append_child(comment);
    } else if (current_node()) {
        current_node()->append_child(comment);
    } else {
        m_document->append_child(comment);
    }
}

dom::Element* TreeBuilder::current_node() const {
    if (m_open_elements.empty()) return nullptr;
    return m_open_elements.back().get();
}

dom::Element* TreeBuilder::adjusted_current_node() const {
    if (m_context_element && m_open_elements.size() == 1) {
        return m_context_element;
    }
    return current_node();
}

void TreeBuilder::push_open_element(RefPtr<dom::Element> element) {
    m_open_elements.push_back(element);
}

void TreeBuilder::pop_current_element() {
    if (!m_open_elements.empty()) {
        m_open_elements.pop_back();
    }
}

void TreeBuilder::remove_from_stack(dom::Element* element) {
    m_open_elements.erase(
        std::remove_if(m_open_elements.begin(), m_open_elements.end(),
            [element](const RefPtr<dom::Element>& e) { return e.get() == element; }),
        m_open_elements.end());
}

bool TreeBuilder::stack_contains(const String& tag_name) const {
    for (const auto& elem : m_open_elements) {
        if (elem->local_name() == tag_name) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_scope(const String& tag_name) const {
    static const std::array<const char*, 18> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template",
        "foreignObject", "desc", "title",
        "mi", "mo", "mn", "ms", "mtext", "annotation-xml"
    };

    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        for (const char* marker : scope_markers) {
            if ((*it)->local_name() == String(marker)) {
                return false;
            }
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_list_item_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "ol"_s || (*it)->local_name() == "ul"_s) {
            return false;
        }
    }
    return stack_contains_in_scope(tag_name);
}

bool TreeBuilder::stack_contains_in_button_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "button"_s) {
            return false;
        }
    }
    return stack_contains_in_scope(tag_name);
}

bool TreeBuilder::stack_contains_in_table_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "html"_s || (*it)->local_name() == "table"_s ||
            (*it)->local_name() == "template"_s) {
            return false;
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_select_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() != "optgroup"_s && (*it)->local_name() != "option"_s) {
            return false;
        }
    }
    return false;
}

void TreeBuilder::push_active_formatting_element(RefPtr<dom::Element> element, const Token& token) {
    // Limit to three entries with same tag name as per spec.
    usize count = 0;
    std::optional<usize> first_match;
    for (usize i = 0; i < m_active_formatting_elements.size(); ++i) {
        auto& afe = m_active_formatting_elements[i];
        if (afe.type == ActiveFormattingElement::Type::Element &&
            afe.element && afe.element->local_name() == element->local_name()) {
            if (!first_match.has_value()) {
                first_match = i;
            }
            ++count;
        }
    }
    if (count >= 3 && first_match.has_value()) {
        m_active_formatting_elements.erase(m_active_formatting_elements.begin() + static_cast<isize>(*first_match));
    }

    m_active_formatting_elements.push_back({
        ActiveFormattingElement::Type::Element,
        element,
        token
    });
}

void TreeBuilder::push_marker() {
    m_active_formatting_elements.push_back(ActiveFormattingElement::marker());
}

void TreeBuilder::reconstruct_active_formatting_elements() {
    if (m_active_formatting_elements.empty()) return;

    auto& last = m_active_formatting_elements.back();
    if (last.type == ActiveFormattingElement::Type::Marker) return;

    for (const auto& elem : m_open_elements) {
        if (elem.get() == last.element.get()) {
            return;
        }
    }

    for (auto it = m_active_formatting_elements.rbegin(); it != m_active_formatting_elements.rend(); ++it) {
        if (it->type == ActiveFormattingElement::Type::Marker) break;

        bool on_stack = false;
        for (const auto& elem : m_open_elements) {
            if (elem.get() == it->element.get()) {
                on_stack = true;
                break;
            }
        }

        if (!on_stack) {
            auto& tag = std::get<TagToken>(it->token);
            auto element = create_element_for_token(tag);
            insert_element(element);
            it->element = element;
        }
    }
}

void TreeBuilder::clear_active_formatting_to_last_marker() {
    while (!m_active_formatting_elements.empty()) {
        auto& last = m_active_formatting_elements.back();
        if (last.type == ActiveFormattingElement::Type::Marker) {
            m_active_formatting_elements.pop_back();
            break;
        }
        m_active_formatting_elements.pop_back();
    }
}

void TreeBuilder::remove_from_active_formatting(dom::Element* element) {
    m_active_formatting_elements.erase(
        std::remove_if(m_active_formatting_elements.begin(), m_active_formatting_elements.end(),
            [element](const ActiveFormattingElement& afe) {
                return afe.element.get() == element;
            }),
        m_active_formatting_elements.end());
}

void TreeBuilder::adoption_agency_algorithm(const String& tag_name) {
    for (int iteration = 0; iteration < 8; ++iteration) {
        // 1. Find formatting element
        auto afe_it = std::find_if(m_active_formatting_elements.rbegin(),
            m_active_formatting_elements.rend(),
            [&](const ActiveFormattingElement& afe) {
                return afe.type == ActiveFormattingElement::Type::Element &&
                    afe.element && afe.element->local_name() == tag_name;
            });

        if (afe_it == m_active_formatting_elements.rend()) {
            return;
        }

        auto* formatting_element = afe_it->element.get();
        auto active_it = std::next(afe_it).base();

        // 2. If formatting element not on stack, remove from active list
        auto stack_it = std::find_if(m_open_elements.begin(), m_open_elements.end(),
            [&](const RefPtr<dom::Element>& el) { return el.get() == formatting_element; });
        if (stack_it == m_open_elements.end()) {
            parse_error("adoption-agency-formatting-not-on-stack"_s);
            m_active_formatting_elements.erase(active_it);
            return;
        }

        if (!stack_contains_in_scope(tag_name)) {
            parse_error("adoption-agency-no-scope"_s);
            m_active_formatting_elements.erase(active_it);
            return;
        }

        if (current_node() != formatting_element) {
            parse_error("adoption-agency-misnested"_s);
        }

        // 3. Find furthest block
        dom::Element* furthest_block = nullptr;
        auto formatting_index = static_cast<int>(stack_it - m_open_elements.begin());
        int furthest_index = -1;
        for (int i = formatting_index + 1; i < static_cast<int>(m_open_elements.size()); ++i) {
            auto name = m_open_elements[i]->local_name();
            if (is_special_element(name)) {
                furthest_block = m_open_elements[i].get();
                furthest_index = i;
                break;
            }
        }

        if (!furthest_block) {
            // Pop formatting element and remove from active formatting
            m_open_elements.erase(stack_it);
            m_active_formatting_elements.erase(active_it);
            return;
        }

        auto* common_ancestor = (formatting_index > 0)
            ? m_open_elements[formatting_index - 1].get()
            : nullptr;

        dom::Element* furthest_block_node = furthest_block;
        dom::Element* last_node = furthest_block;

        // 4. Walk back from furthest block to formatting element
        for (int i = furthest_index - 1; i >= formatting_index; --i) {
            auto* current = m_open_elements[i].get();

            auto afe_match = std::find_if(m_active_formatting_elements.begin(),
                m_active_formatting_elements.end(),
                [&](const ActiveFormattingElement& afe) {
                    return afe.type == ActiveFormattingElement::Type::Element &&
                        afe.element && afe.element.get() == current;
                });

            if (afe_match == m_active_formatting_elements.end()) {
                m_open_elements.erase(m_open_elements.begin() + i);
                continue;
            }

            if (current == formatting_element) {
                break;
            }

            auto clone = create_element_for_token(std::get<TagToken>(afe_match->token));
            afe_match->element = clone;
            m_open_elements[i] = clone;

            if (last_node->parent_node()) {
                last_node->parent_node()->remove_child(RefPtr<dom::Node>(last_node));
            }
            clone->append_child(RefPtr<dom::Node>(last_node));
            last_node = clone.get();
        }

        // 5. Reparent last_node to common ancestor
        if (common_ancestor) {
            if (last_node->parent_node()) {
                last_node->parent_node()->remove_child(RefPtr<dom::Node>(last_node));
            }
            if (common_ancestor->local_name() == "table"_s) {
                if (auto* parent = common_ancestor->parent_node()) {
                    parent->insert_before(RefPtr<dom::Node>(last_node), common_ancestor);
                }
            } else {
                common_ancestor->append_child(RefPtr<dom::Node>(last_node));
            }
        }

        // 6. Create new formatting element and move children
        auto formatting_token = std::get<TagToken>(active_it->token);
        auto new_formatting_element = create_element_for_token(formatting_token);
        while (furthest_block_node->first_child()) {
            auto child = RefPtr<dom::Node>(furthest_block_node->first_child());
            furthest_block_node->remove_child(child);
            new_formatting_element->append_child(child);
        }
        furthest_block_node->append_child(new_formatting_element);

        // 7. Update stacks
        m_open_elements.erase(stack_it);
        m_open_elements.insert(m_open_elements.begin() + formatting_index, new_formatting_element);

        m_active_formatting_elements.erase(active_it);
        m_active_formatting_elements.push_back({ActiveFormattingElement::Type::Element, new_formatting_element, formatting_token});

        return;
    }
}

TreeBuilder::InsertionLocation TreeBuilder::appropriate_insertion_place() {
    if (!m_foster_parenting) {
        return {adjusted_current_node(), nullptr};
    }

    int last_template_index = -1;
    int last_table_index = -1;
    for (int i = static_cast<int>(m_open_elements.size()) - 1; i >= 0; --i) {
        auto name = m_open_elements[static_cast<usize>(i)]->local_name();
        if (last_template_index == -1 && name == "template"_s) {
            last_template_index = i;
        }
        if (last_table_index == -1 && name == "table"_s) {
            last_table_index = i;
        }
        if (last_template_index != -1 && last_table_index != -1) {
            break;
        }
    }

    bool use_table = last_table_index != -1 &&
        (last_template_index == -1 || last_table_index > last_template_index);

    if (use_table) {
        auto* table = m_open_elements[static_cast<usize>(last_table_index)].get();
        if (auto* parent = table->parent_node()) {
            return {parent, table};
        }
        if (last_table_index > 0) {
            return {m_open_elements[static_cast<usize>(last_table_index - 1)].get(), nullptr};
        }
        return {table, nullptr};
    }

    if (last_template_index != -1) {
        auto* template_element = m_open_elements[static_cast<usize>(last_template_index)].get();
        return {template_element, nullptr};
    }

    return {adjusted_current_node(), nullptr};
}

void TreeBuilder::generate_implied_end_tags(const String& except) {
    while (current_node()) {
        auto name = current_node()->local_name();
        if (name == except) break;

        bool is_implied = false;
        for (const char* tag : detail::IMPLIED_END_TAG_ELEMENTS) {
            if (name == String(tag)) {
                is_implied = true;
                break;
            }
        }

        if (!is_implied) break;
        pop_current_element();
    }
}

void TreeBuilder::generate_all_implied_end_tags_thoroughly() {
    while (current_node()) {
        auto name = current_node()->local_name();

        bool is_implied = false;
        for (const char* tag : detail::IMPLIED_END_TAG_ELEMENTS) {
            if (name == String(tag)) {
                is_implied = true;
                break;
            }
        }
        if (name == "caption"_s || name == "colgroup"_s || name == "tbody"_s ||
            name == "td"_s || name == "tfoot"_s || name == "th"_s ||
            name == "thead"_s || name == "tr"_s) {
            is_implied = true;
        }

        if (!is_implied) break;
        pop_current_element();
    }
}

bool TreeBuilder::is_special_element(const String& tag_name) {
    for (const char* special : detail::SPECIAL_ELEMENTS) {
        if (tag_name == String(special)) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::is_formatting_element(const String& tag_name) {
    for (const char* fmt : detail::FORMATTING_ELEMENTS) {
        if (tag_name == String(fmt)) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::is_form_associated(const String& tag_name) {
    static const std::array<const char*, 12> FORM_ASSOCIATED = {
        "button", "fieldset", "input", "label", "object", "output",
        "select", "textarea", "option", "optgroup", "meter", "progress"
    };
    for (const char* name : FORM_ASSOCIATED) {
        if (tag_name == String(name)) {
            return true;
        }
    }
    return false;
}

void TreeBuilder::parse_error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message);
    }
}

bool TreeBuilder::set_insertion_mode_if_allowed(InsertionMode mode, const String& reason) {
    bool locked = m_parser_cannot_change_mode && !m_context_element;
    if (locked && mode != m_insertion_mode && mode != InsertionMode::Text) {
        parse_error(reason.empty() ? "parser-cannot-change-mode"_s : reason);
        return false;
    }
    m_insertion_mode = mode;
    return true;
}

void TreeBuilder::reset_insertion_mode_appropriately() {
    if (m_parser_cannot_change_mode && !m_context_element) {
        parse_error("parser-cannot-change-mode"_s);
        return;
    }
    bool last = false;
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        auto* node = it->get();

        if (it == m_open_elements.rend() - 1) {
            last = true;
            if (m_context_element) {
                node = m_context_element;
            }
        }

        auto name = node->local_name();

        if (name == "select"_s) {
            m_insertion_mode = InsertionMode::InSelect;
            return;
        }
        if (name == "td"_s || name == "th"_s) {
            if (!last) {
                m_insertion_mode = InsertionMode::InCell;
                return;
            }
        }
        if (name == "tr"_s) {
            m_insertion_mode = InsertionMode::InRow;
            return;
        }
        if (name == "tbody"_s || name == "thead"_s || name == "tfoot"_s) {
            m_insertion_mode = InsertionMode::InTableBody;
            return;
        }
        if (name == "caption"_s) {
            m_insertion_mode = InsertionMode::InCaption;
            return;
        }
        if (name == "colgroup"_s) {
            m_insertion_mode = InsertionMode::InColumnGroup;
            return;
        }
        if (name == "table"_s) {
            m_insertion_mode = InsertionMode::InTable;
            return;
        }
        if (name == "template"_s) {
            m_insertion_mode = m_template_insertion_modes.empty()
                ? InsertionMode::InTemplate
                : m_template_insertion_modes.top();
            return;
        }
        if (name == "head"_s) {
            if (!last) {
                m_insertion_mode = InsertionMode::InHead;
                return;
            }
        }
        if (name == "body"_s) {
            m_insertion_mode = InsertionMode::InBody;
            return;
        }
        if (name == "frameset"_s) {
            m_insertion_mode = InsertionMode::InFrameset;
            return;
        }
        if (name == "html"_s) {
            if (!m_head_element) {
                m_insertion_mode = InsertionMode::BeforeHead;
            } else {
                m_insertion_mode = InsertionMode::AfterHead;
            }
            return;
        }
        if (last) {
            m_insertion_mode = InsertionMode::InBody;
            return;
        }
    }
}

void TreeBuilder::associate_form_owner(dom::Element* element, const TagToken& token) {
    if (!element || !is_form_associated(element->local_name())) {
        return;
    }

    if (stack_contains("template"_s)) {
        return;
    }

    dom::Element* owner = nullptr;
    if (auto form_attr = token.get_attribute("form"_s)) {
        owner = m_document ? m_document->get_element_by_id(*form_attr) : nullptr;
        if (owner && owner->local_name() != "form"_s) {
            owner = nullptr;
        }
        if (!owner) {
            m_pending_form_associations[*form_attr].push_back(element);
            element->set_form_owner(nullptr);
            return;
        }
    } else if (m_form_element) {
        owner = m_form_element;
    } else if (element->local_name() == "option"_s || element->local_name() == "optgroup"_s) {
        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            if ((*it)->local_name() == "select"_s) {
                owner = (*it)->form_owner();
                break;
            }
        }
    }

    if (owner) {
        element->set_form_owner(owner);
    } else {
        element->set_form_owner(nullptr);
    }
}

void TreeBuilder::resolve_pending_form_controls(dom::Element* form) {
    if (!form) return;
    auto id_attr = form->get_attribute("id"_s);
    if (!id_attr.has_value()) return;
    auto it = m_pending_form_associations.find(*id_attr);
    if (it == m_pending_form_associations.end()) return;
    for (auto* control : it->second) {
        if (control) {
            control->set_form_owner(form);
        }
    }
    m_pending_form_associations.erase(it);
}

void TreeBuilder::refresh_form_owner_for_subtree(dom::Node* node) {
    if (!node) return;
    if (node->is_element()) {
        auto* element = node->as_element();
        if (is_form_associated(element->local_name())) {
            TagToken pseudo;
            pseudo.name = element->local_name();
            for (const auto& attr : element->attributes()) {
                pseudo.set_attribute(attr.name, attr.value);
            }
            associate_form_owner(element, pseudo);
        }
    }
    for (const auto& child : node->child_nodes()) {
        refresh_form_owner_for_subtree(child.get());
    }
}

bool TreeBuilder::process_foreign_content(const Token& token) {
    if (!in_foreign_content()) {
        return false;
    }

    static const std::array<const char*, 48> HTML_BREAKOUT = {
        "b", "big", "blockquote", "body", "br", "center", "code", "dd", "div", "dl",
        "dt", "em", "embed", "h1", "h2", "h3", "h4", "h5", "h6", "head", "hr", "i",
        "html", "img", "li", "listing", "menu", "meta", "nav", "ol", "p", "pre", "ruby",
        "section", "small", "span", "strong", "summary", "table", "tbody",
        "td", "template", "tfoot", "th", "thead", "title", "tr", "ul"
    };

    if (is_start_tag(token)) {
        auto name_lower = std::get<TagToken>(token).name.to_lowercase();
        for (const auto* html_tag : HTML_BREAKOUT) {
            if (name_lower == String(html_tag)) {
                return false;
            }
        }
    }

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        auto cp = character->code_point;
        if (cp == 0) {
            parse_error("unexpected-null-character"_s);
            cp = unicode::REPLACEMENT_CHARACTER;
        }
        insert_character(cp);
        return true;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return true;
    }

    if (std::holds_alternative<DoctypeToken>(token)) {
        parse_error("Unexpected DOCTYPE in foreign content"_s);
        return true;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);

        auto local = element->local_name().to_lowercase();
        static const String SVG_NS = "http://www.w3.org/2000/svg"_s;
        if (element->namespace_uri() == SVG_NS) {
            if (local == "script"_s && m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::ScriptData);
                m_original_insertion_mode = m_insertion_mode;
                m_insertion_mode = InsertionMode::Text;
            } else if (local == "style"_s && m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RAWTEXT);
                m_original_insertion_mode = m_insertion_mode;
                m_insertion_mode = InsertionMode::Text;
            }
        }

        if (tag.self_closing) {
            acknowledge_self_closing_flag();
            pop_current_element();
        }
        return true;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        auto target = tag.name;
        static const String SVG_NS = "http://www.w3.org/2000/svg"_s;
        static const String MATHML_NS = "http://www.w3.org/1998/Math/MathML"_s;

        auto* adjusted = adjusted_current_node();
        auto adjusted_ns = adjusted ? adjusted->namespace_uri() : String();
        if (adjusted_ns == SVG_NS) {
            target = svg_camel_case(tag.name.to_lowercase());
        } else if (adjusted_ns == MATHML_NS) {
            target = tag.name.to_lowercase();
        }

        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            auto* node = it->get();
            if (node->namespace_uri().empty()) {
                break;
            }
            if (node->local_name() == target) {
                while (current_node() && current_node() != node) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
                return true;
            }
        }
        return false;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return false;
    }

    return false;
}

} // namespace lithium::html
