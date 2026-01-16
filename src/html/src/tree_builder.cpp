/**
 * HTML Tree Builder implementation
 * Following WHATWG HTML Living Standard
 */

#include "lithium/html/tree_builder.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>
#include <array>

namespace lithium::html {

// ============================================================================
// Special elements list
// ============================================================================

static constexpr std::array<const char*, 68> SPECIAL_ELEMENTS = {
    "address", "applet", "area", "article", "aside", "base", "basefont",
    "bgsound", "blockquote", "body", "br", "button", "caption", "center",
    "col", "colgroup", "dd", "details", "dir", "div", "dl", "dt", "embed",
    "fieldset", "figcaption", "figure", "footer", "form", "frame", "frameset",
    "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hgroup", "hr",
    "html", "iframe", "img", "input", "keygen", "li", "link", "listing",
    "main", "marquee", "menu", "meta", "nav", "noembed", "noframes",
    "noscript", "object", "ol", "p", "param", "plaintext", "pre", "script",
    "section", "select", "source", "style", "summary", "table", "tbody",
    "td", "template", "textarea", "tfoot", "th", "thead", "title", "tr",
    "track", "ul", "wbr", "xmp"
};

static constexpr std::array<const char*, 14> FORMATTING_ELEMENTS = {
    "a", "b", "big", "code", "em", "font", "i", "nobr", "s", "small",
    "strike", "strong", "tt", "u"
};

static constexpr std::array<const char*, 12> IMPLIED_END_TAG_ELEMENTS = {
    "dd", "dt", "li", "optgroup", "option", "p", "rb", "rp", "rt", "rtc"
};

// ============================================================================
// TreeBuilder implementation
// ============================================================================

TreeBuilder::TreeBuilder() = default;

void TreeBuilder::set_document(RefPtr<dom::Document> document) {
    m_document = document;
}

void TreeBuilder::process_token(const Token& token) {
    // Handle according to current insertion mode
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
        case InsertionMode::AfterBody:
            process_after_body(token);
            break;
        case InsertionMode::AfterAfterBody:
            process_after_after_body(token);
            break;
        default:
            process_in_body(token);
            break;
    }
}

// ============================================================================
// Insertion mode handlers
// ============================================================================

void TreeBuilder::process_initial(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            // Ignore whitespace
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        auto dt = m_document->create_document_type(
            doctype->name,
            doctype->public_identifier.value_or(String()),
            doctype->system_identifier.value_or(String())
        );
        m_document->append_child(dt);

        if (doctype->force_quirks) {
            m_document->set_quirks_mode(dom::Document::QuirksMode::Quirks);
        }

        m_insertion_mode = InsertionMode::BeforeHtml;
        return;
    }

    // Anything else
    m_document->set_quirks_mode(dom::Document::QuirksMode::Quirks);
    m_insertion_mode = InsertionMode::BeforeHtml;
    process_token(token);
}

void TreeBuilder::process_before_html(const Token& token) {
    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            return;
        }
    }

    if (is_start_tag_named(token, "html"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        m_document->append_child(element);
        push_open_element(element);
        m_insertion_mode = InsertionMode::BeforeHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "head"_s && tag.name != "body"_s &&
            tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    // Anything else: create html element
    auto html = m_document->create_element("html"_s);
    m_document->append_child(html);
    push_open_element(html);
    m_insertion_mode = InsertionMode::BeforeHead;
    process_token(token);
}

void TreeBuilder::process_before_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "head"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto head = create_element_for_token(tag);
        insert_element(head);
        m_head_element = head.get();
        m_insertion_mode = InsertionMode::InHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "head"_s && tag.name != "body"_s &&
            tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    // Anything else: create head element
    auto head = m_document->create_element("head"_s);
    insert_element(head);
    m_head_element = head.get();
    m_insertion_mode = InsertionMode::InHead;
    process_token(token);
}

void TreeBuilder::process_in_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            insert_character(character->code_point);
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            return;
        }

        if (tag.name == "meta"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            return;
        }

        if (tag.name == "title"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RCDATA);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "style"_s || tag.name == "noscript"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RAWTEXT);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "script"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::ScriptData);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "head"_s) {
            parse_error("Unexpected head tag"_s);
            return;
        }
    }

    if (is_end_tag_named(token, "head"_s)) {
        pop_current_element();
        m_insertion_mode = InsertionMode::AfterHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "body"_s && tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    // Anything else
    pop_current_element();
    m_insertion_mode = InsertionMode::AfterHead;
    process_token(token);
}

void TreeBuilder::process_in_head_noscript(const Token& token) {
    // Simplified: just switch to in head
    process_in_head(token);
}

void TreeBuilder::process_after_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            insert_character(character->code_point);
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "body"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::InBody;
        return;
    }

    if (is_start_tag_named(token, "frameset"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_insertion_mode = InsertionMode::InFrameset;
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s ||
            tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "script"_s || tag.name == "style"_s ||
            tag.name == "template"_s || tag.name == "title"_s) {
            parse_error("Unexpected tag in after head"_s);
            push_open_element(RefPtr<dom::Element>(m_head_element));
            process_using_rules_for(InsertionMode::InHead, token);
            remove_from_stack(m_head_element);
            return;
        }

        if (tag.name == "head"_s) {
            parse_error("Unexpected head tag"_s);
            return;
        }
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }
        if (tag.name != "body"_s && tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    // Anything else: create body element
    auto body = m_document->create_element("body"_s);
    insert_element(body);
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::process_in_body(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == 0) {
            parse_error("Unexpected null character"_s);
            return;
        }

        reconstruct_active_formatting_elements();
        insert_character(character->code_point);

        if (character->code_point != '\t' && character->code_point != '\n' &&
            character->code_point != '\f' && character->code_point != '\r' &&
            character->code_point != ' ') {
            m_frameset_ok = false;
        }
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        parse_error("Unexpected html tag"_s);
        // Merge attributes
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);

        // Handle various start tags
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s ||
            tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "script"_s || tag.name == "style"_s ||
            tag.name == "template"_s || tag.name == "title"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }

        if (tag.name == "body"_s) {
            parse_error("Unexpected body tag"_s);
            return;
        }

        if (tag.name == "frameset"_s) {
            parse_error("Unexpected frameset tag"_s);
            return;
        }

        // Block elements
        if (tag.name == "address"_s || tag.name == "article"_s ||
            tag.name == "aside"_s || tag.name == "blockquote"_s ||
            tag.name == "center"_s || tag.name == "details"_s ||
            tag.name == "dialog"_s || tag.name == "dir"_s ||
            tag.name == "div"_s || tag.name == "dl"_s ||
            tag.name == "fieldset"_s || tag.name == "figcaption"_s ||
            tag.name == "figure"_s || tag.name == "footer"_s ||
            tag.name == "header"_s || tag.name == "hgroup"_s ||
            tag.name == "main"_s || tag.name == "menu"_s ||
            tag.name == "nav"_s || tag.name == "ol"_s ||
            tag.name == "p"_s || tag.name == "section"_s ||
            tag.name == "summary"_s || tag.name == "ul"_s) {
            if (stack_contains_in_button_scope("p"_s)) {
                // Close p element
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        // Headings
        if (tag.name == "h1"_s || tag.name == "h2"_s ||
            tag.name == "h3"_s || tag.name == "h4"_s ||
            tag.name == "h5"_s || tag.name == "h6"_s) {
            if (stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        // Pre, listing
        if (tag.name == "pre"_s || tag.name == "listing"_s) {
            if (stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_frameset_ok = false;
            return;
        }

        // Form
        if (tag.name == "form"_s) {
            if (m_form_element && !stack_contains("template"_s)) {
                parse_error("Form already open"_s);
                return;
            }
            if (stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (!stack_contains("template"_s)) {
                m_form_element = element.get();
            }
            return;
        }

        // Li
        if (tag.name == "li"_s) {
            m_frameset_ok = false;
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        // Dd, dt
        if (tag.name == "dd"_s || tag.name == "dt"_s) {
            m_frameset_ok = false;
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        // Formatting elements
        if (tag.name == "a"_s) {
            // Check for existing a in active formatting
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_active_formatting_element(element, token);
            return;
        }

        if (tag.name == "b"_s || tag.name == "big"_s || tag.name == "code"_s ||
            tag.name == "em"_s || tag.name == "font"_s || tag.name == "i"_s ||
            tag.name == "s"_s || tag.name == "small"_s || tag.name == "strike"_s ||
            tag.name == "strong"_s || tag.name == "tt"_s || tag.name == "u"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_active_formatting_element(element, token);
            return;
        }

        // Void elements
        if (tag.name == "area"_s || tag.name == "br"_s || tag.name == "embed"_s ||
            tag.name == "img"_s || tag.name == "keygen"_s || tag.name == "wbr"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            m_frameset_ok = false;
            return;
        }

        if (tag.name == "input"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            auto type_attr = tag.get_attribute("type"_s);
            if (!type_attr || type_attr->to_lowercase() != "hidden"_s) {
                m_frameset_ok = false;
            }
            return;
        }

        if (tag.name == "hr"_s) {
            if (stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            m_frameset_ok = false;
            return;
        }

        // Textarea
        if (tag.name == "textarea"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RCDATA);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        // Table
        if (tag.name == "table"_s) {
            if (m_document->quirks_mode() != dom::Document::QuirksMode::Quirks &&
                stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::InTable;
            return;
        }

        // Default: any other start tag
        reconstruct_active_formatting_elements();
        auto element = create_element_for_token(tag);
        insert_element(element);
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);

        if (tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }

        if (tag.name == "body"_s) {
            if (!stack_contains("body"_s)) {
                parse_error("No body to close"_s);
                return;
            }
            m_insertion_mode = InsertionMode::AfterBody;
            return;
        }

        if (tag.name == "html"_s) {
            if (!stack_contains("body"_s)) {
                parse_error("No body to close"_s);
                return;
            }
            m_insertion_mode = InsertionMode::AfterBody;
            process_token(token);
            return;
        }

        // Block closing
        if (tag.name == "address"_s || tag.name == "article"_s ||
            tag.name == "aside"_s || tag.name == "blockquote"_s ||
            tag.name == "button"_s || tag.name == "center"_s ||
            tag.name == "details"_s || tag.name == "dialog"_s ||
            tag.name == "dir"_s || tag.name == "div"_s ||
            tag.name == "dl"_s || tag.name == "fieldset"_s ||
            tag.name == "figcaption"_s || tag.name == "figure"_s ||
            tag.name == "footer"_s || tag.name == "header"_s ||
            tag.name == "hgroup"_s || tag.name == "listing"_s ||
            tag.name == "main"_s || tag.name == "menu"_s ||
            tag.name == "nav"_s || tag.name == "ol"_s ||
            tag.name == "pre"_s || tag.name == "section"_s ||
            tag.name == "summary"_s || tag.name == "ul"_s) {
            if (!stack_contains_in_scope(tag.name)) {
                parse_error("No matching tag in scope"_s);
                return;
            }
            generate_implied_end_tags();
            while (current_node() && current_node()->local_name() != tag.name) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        if (tag.name == "form"_s) {
            if (!stack_contains("template"_s)) {
                auto* form = m_form_element;
                m_form_element = nullptr;
                if (!form || !stack_contains_in_scope("form"_s)) {
                    parse_error("No form to close"_s);
                    return;
                }
                generate_implied_end_tags();
                remove_from_stack(form);
            }
            return;
        }

        if (tag.name == "p"_s) {
            if (!stack_contains_in_button_scope("p"_s)) {
                parse_error("No p to close"_s);
                auto p = m_document->create_element("p"_s);
                insert_element(p);
            }
            while (current_node() && current_node()->local_name() != "p"_s) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        // Li
        if (tag.name == "li"_s) {
            if (!stack_contains_in_list_item_scope("li"_s)) {
                parse_error("No li to close"_s);
                return;
            }
            generate_implied_end_tags("li"_s);
            while (current_node() && current_node()->local_name() != "li"_s) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        // Dd, dt
        if (tag.name == "dd"_s || tag.name == "dt"_s) {
            if (!stack_contains_in_scope(tag.name)) {
                parse_error("No matching tag to close"_s);
                return;
            }
            generate_implied_end_tags(tag.name);
            while (current_node() && current_node()->local_name() != tag.name) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        // Headings
        if (tag.name == "h1"_s || tag.name == "h2"_s ||
            tag.name == "h3"_s || tag.name == "h4"_s ||
            tag.name == "h5"_s || tag.name == "h6"_s) {
            if (!stack_contains_in_scope("h1"_s) &&
                !stack_contains_in_scope("h2"_s) &&
                !stack_contains_in_scope("h3"_s) &&
                !stack_contains_in_scope("h4"_s) &&
                !stack_contains_in_scope("h5"_s) &&
                !stack_contains_in_scope("h6"_s)) {
                parse_error("No heading to close"_s);
                return;
            }
            generate_implied_end_tags();
            while (current_node()) {
                auto name = current_node()->local_name();
                pop_current_element();
                if (name == "h1"_s || name == "h2"_s || name == "h3"_s ||
                    name == "h4"_s || name == "h5"_s || name == "h6"_s) {
                    break;
                }
            }
            return;
        }

        // Formatting elements
        if (tag.name == "a"_s || tag.name == "b"_s || tag.name == "big"_s ||
            tag.name == "code"_s || tag.name == "em"_s || tag.name == "font"_s ||
            tag.name == "i"_s || tag.name == "nobr"_s || tag.name == "s"_s ||
            tag.name == "small"_s || tag.name == "strike"_s ||
            tag.name == "strong"_s || tag.name == "tt"_s || tag.name == "u"_s) {
            adoption_agency_algorithm(tag.name);
            return;
        }

        // Any other end tag
        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            auto* node = it->get();
            if (node->local_name() == tag.name) {
                generate_implied_end_tags(tag.name);
                while (current_node() != node) {
                    pop_current_element();
                }
                pop_current_element();
                return;
            }
            if (is_special_element(node->local_name())) {
                parse_error("Unexpected end tag"_s);
                return;
            }
        }
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        // Stop parsing
        return;
    }
}

void TreeBuilder::process_text(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        insert_character(character->code_point);
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        parse_error("Unexpected EOF in text"_s);
        pop_current_element();
        m_insertion_mode = m_original_insertion_mode;
        process_token(token);
        return;
    }

    if (is_end_tag(token)) {
        pop_current_element();
        m_insertion_mode = m_original_insertion_mode;
        return;
    }
}

void TreeBuilder::process_in_table(const Token& token) {
    // Simplified table handling
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "caption"_s || tag.name == "colgroup"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (tag.name == "tbody"_s || tag.name == "tfoot"_s ||
                tag.name == "thead"_s) {
                m_insertion_mode = InsertionMode::InTableBody;
            }
            return;
        }
        if (tag.name == "tr"_s) {
            auto tbody = m_document->create_element("tbody"_s);
            insert_element(tbody);
            m_insertion_mode = InsertionMode::InTableBody;
            process_token(token);
            return;
        }
    }

    if (is_end_tag_named(token, "table"_s)) {
        while (current_node() && current_node()->local_name() != "table"_s) {
            pop_current_element();
        }
        if (current_node()) {
            pop_current_element();
        }
        reset_insertion_mode_appropriately();
        return;
    }

    // Foster parenting for other content
    m_foster_parenting = true;
    process_using_rules_for(InsertionMode::InBody, token);
    m_foster_parenting = false;
}

void TreeBuilder::process_in_table_text(const Token& token) {
    process_in_table(token);
}

void TreeBuilder::process_in_caption(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_in_column_group(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_in_table_body(const Token& token) {
    if (is_start_tag_named(token, "tr"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_insertion_mode = InsertionMode::InRow;
        return;
    }
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "th"_s || tag.name == "td"_s) {
            auto tr = m_document->create_element("tr"_s);
            insert_element(tr);
            m_insertion_mode = InsertionMode::InRow;
            process_token(token);
            return;
        }
    }
    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
            return;
        }
        if (tag.name == "table"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
            process_token(token);
            return;
        }
    }
    process_in_table(token);
}

void TreeBuilder::process_in_row(const Token& token) {
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "th"_s || tag.name == "td"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_insertion_mode = InsertionMode::InCell;
            push_marker();
            return;
        }
    }
    if (is_end_tag_named(token, "tr"_s)) {
        pop_current_element();
        m_insertion_mode = InsertionMode::InTableBody;
        return;
    }
    process_in_table(token);
}

void TreeBuilder::process_in_cell(const Token& token) {
    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "td"_s || tag.name == "th"_s) {
            pop_current_element();
            clear_active_formatting_to_last_marker();
            m_insertion_mode = InsertionMode::InRow;
            return;
        }
    }
    process_in_body(token);
}

void TreeBuilder::process_in_select(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_in_select_in_table(const Token& token) {
    process_in_select(token);
}

void TreeBuilder::process_in_template(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_after_body(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            process_using_rules_for(InsertionMode::InBody, token);
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_end_tag_named(token, "html"_s)) {
        m_insertion_mode = InsertionMode::AfterAfterBody;
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }

    parse_error("Unexpected token after body"_s);
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::process_in_frameset(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_after_frameset(const Token& token) {
    process_in_body(token);
}

void TreeBuilder::process_after_after_body(const Token& token) {
    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            process_using_rules_for(InsertionMode::InBody, token);
            return;
        }
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }

    parse_error("Unexpected token after after body"_s);
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::process_after_after_frameset(const Token& token) {
    process_after_after_body(token);
}

void TreeBuilder::process_using_rules_for(InsertionMode mode, const Token& token) {
    auto saved_mode = m_insertion_mode;
    m_insertion_mode = mode;
    process_token(token);
    m_insertion_mode = saved_mode;
}

// ============================================================================
// Tree manipulation
// ============================================================================

RefPtr<dom::Element> TreeBuilder::create_element(const TagToken& token, const String& /*namespace_uri*/) {
    auto element = m_document->create_element(token.name);
    for (const auto& [name, value] : token.attributes) {
        element->set_attribute(name, value);
    }
    return element;
}

RefPtr<dom::Element> TreeBuilder::create_element_for_token(const TagToken& token) {
    return create_element(token, String());
}

void TreeBuilder::insert_element(RefPtr<dom::Element> element) {
    auto* insert_at = appropriate_insertion_place();
    if (insert_at) {
        insert_at->append_child(element);
    }
    push_open_element(element);
}

void TreeBuilder::insert_character(unicode::CodePoint cp) {
    auto* insert_at = appropriate_insertion_place();
    if (!insert_at) return;

    // Check if last child is a text node
    auto* last = insert_at->last_child();
    if (last && last->is_text()) {
        auto* text = last->as_text();
        text->append_data(String(1, static_cast<char>(cp)));
    } else {
        auto text = m_document->create_text_node(String(1, static_cast<char>(cp)));
        insert_at->append_child(text);
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

// ============================================================================
// Stack management
// ============================================================================

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
    static const std::array<const char*, 10> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template", "foreignObject"
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

// ============================================================================
// Active formatting elements
// ============================================================================

void TreeBuilder::push_active_formatting_element(RefPtr<dom::Element> element, const Token& token) {
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

    // Check if already on stack
    for (const auto& elem : m_open_elements) {
        if (elem.get() == last.element.get()) {
            return;
        }
    }

    // Reconstruct
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

// ============================================================================
// Adoption agency algorithm
// ============================================================================

void TreeBuilder::adoption_agency_algorithm(const String& tag_name) {
    // Simplified adoption agency
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            // Pop elements until we reach this one
            while (current_node() != it->get()) {
                pop_current_element();
            }
            pop_current_element();
            remove_from_active_formatting(it->get());
            return;
        }
        if (is_special_element((*it)->local_name())) {
            parse_error("Unexpected special element"_s);
            return;
        }
    }
}

// ============================================================================
// Foster parenting
// ============================================================================

dom::Node* TreeBuilder::appropriate_insertion_place() {
    if (m_foster_parenting) {
        // Find last table in stack
        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            if ((*it)->local_name() == "table"_s) {
                if ((*it)->parent_node()) {
                    return (*it)->parent_node();
                }
            }
        }
    }

    return current_node();
}

// ============================================================================
// Implied end tags
// ============================================================================

void TreeBuilder::generate_implied_end_tags(const String& except) {
    while (current_node()) {
        auto name = current_node()->local_name();
        if (name == except) break;

        bool is_implied = false;
        for (const char* tag : IMPLIED_END_TAG_ELEMENTS) {
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
        for (const char* tag : IMPLIED_END_TAG_ELEMENTS) {
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

// ============================================================================
// Special element checks
// ============================================================================

bool TreeBuilder::is_special_element(const String& tag_name) {
    for (const char* special : SPECIAL_ELEMENTS) {
        if (tag_name == String(special)) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::is_formatting_element(const String& tag_name) {
    for (const char* fmt : FORMATTING_ELEMENTS) {
        if (tag_name == String(fmt)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Error reporting
// ============================================================================

void TreeBuilder::parse_error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message);
    }
}

// ============================================================================
// Reset insertion mode
// ============================================================================

void TreeBuilder::reset_insertion_mode_appropriately() {
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

} // namespace lithium::html
