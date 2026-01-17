#pragma once

#include "tokenizer.hpp"
#include "lithium/dom/document.hpp"
#include <stack>
#include <vector>

namespace lithium::html {

// Forward declaration
class TreeBuilder;

// ============================================================================
// Insertion Mode (WHATWG HTML5 spec)
// ============================================================================

enum class InsertionMode {
    Initial,
    BeforeHtml,
    BeforeHead,
    InHead,
    InHeadNoscript,
    AfterHead,
    InBody,
    Text,
    InTable,
    InTableText,
    InCaption,
    InColumnGroup,
    InTableBody,
    InRow,
    InCell,
    InSelect,
    InSelectInTable,
    InTemplate,
    AfterBody,
    InFrameset,
    AfterFrameset,
    AfterAfterBody,
    AfterAfterFrameset,
};

// ============================================================================
// Active Formatting Elements
// ============================================================================

struct ActiveFormattingElement {
    enum class Type { Element, Marker };
    Type type;
    RefPtr<dom::Element> element;
    Token token;  // Original token for reconstruction

    static ActiveFormattingElement marker() {
        return {Type::Marker, nullptr, {}};
    }
};

// ============================================================================
// TreeBuilder - Constructs DOM tree from tokens
// ============================================================================

class TreeBuilder {
public:
    using ErrorCallback = std::function<void(const String& message)>;

    TreeBuilder();

    // Set the document being built
    void set_document(RefPtr<dom::Document> document);

    // Process a token
    void process_token(const Token& token);

    // Get the resulting document
    [[nodiscard]] RefPtr<dom::Document> document() const { return m_document; }

    // Error handling
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }

    // Scripting support
    void set_scripting_enabled(bool enabled) { m_scripting_enabled = enabled; }
    void set_parser_cannot_change_mode(bool value) { m_parser_cannot_change_mode = value; }
    void set_iframe_srcdoc(bool value) { m_is_iframe_srcdoc = value; }

    // Fragment parsing context
    void set_context_element(dom::Element* context) { m_context_element = context; }
    void prepare_for_fragment(RefPtr<dom::Element> context_element);

    // Get tokenizer (for state adjustment)
    [[nodiscard]] Tokenizer* tokenizer() const { return m_tokenizer; }
    void set_tokenizer(Tokenizer* tokenizer) { m_tokenizer = tokenizer; }

private:
    // Insertion mode handlers
    void process_initial(const Token& token);
    void process_before_html(const Token& token);
    void process_before_head(const Token& token);
    void process_in_head(const Token& token);
    void process_in_head_noscript(const Token& token);
    void process_after_head(const Token& token);
    void process_in_body(const Token& token);
    void process_text(const Token& token);
    void process_in_table(const Token& token);
    void process_in_table_text(const Token& token);
    void process_in_caption(const Token& token);
    void process_in_column_group(const Token& token);
    void process_in_table_body(const Token& token);
    void process_in_row(const Token& token);
    void process_in_cell(const Token& token);
    void process_in_select(const Token& token);
    void process_in_select_in_table(const Token& token);
    void process_in_template(const Token& token);
    void process_after_body(const Token& token);
    void process_in_frameset(const Token& token);
    void process_after_frameset(const Token& token);
    void process_after_after_body(const Token& token);
    void process_after_after_frameset(const Token& token);

    // Using the rules for
    void process_using_rules_for(InsertionMode mode, const Token& token);

    // Tree manipulation
    RefPtr<dom::Element> create_element(const TagToken& token, const String& namespace_uri);
    RefPtr<dom::Element> create_element_for_token(const TagToken& token);
    void insert_element(RefPtr<dom::Element> element);
    void insert_character(unicode::CodePoint cp);
    void insert_comment(const CommentToken& token, dom::Node* position = nullptr);

    // Stack of open elements
    [[nodiscard]] dom::Element* current_node() const;
    [[nodiscard]] dom::Element* adjusted_current_node() const;
    void push_open_element(RefPtr<dom::Element> element);
    void pop_current_element();
    void remove_from_stack(dom::Element* element);
    [[nodiscard]] bool stack_contains(const String& tag_name) const;
    [[nodiscard]] bool stack_contains_in_scope(const String& tag_name) const;
    [[nodiscard]] bool stack_contains_in_list_item_scope(const String& tag_name) const;
    [[nodiscard]] bool stack_contains_in_button_scope(const String& tag_name) const;
    [[nodiscard]] bool stack_contains_in_table_scope(const String& tag_name) const;
    [[nodiscard]] bool stack_contains_in_select_scope(const String& tag_name) const;

// Active formatting elements
void push_active_formatting_element(RefPtr<dom::Element> element, const Token& token);
    void push_marker();
    void reconstruct_active_formatting_elements();
    void clear_active_formatting_to_last_marker();
    void remove_from_active_formatting(dom::Element* element);

    // Adoption agency algorithm
    void adoption_agency_algorithm(const String& tag_name);

    // Foster parenting
    void set_foster_parenting(bool enabled) { m_foster_parenting = enabled; }
    struct InsertionLocation {
        dom::Node* parent{nullptr};
        dom::Node* insert_before{nullptr};
    };
    [[nodiscard]] InsertionLocation appropriate_insertion_place();

    // Implicit end tags
    void generate_implied_end_tags(const String& except = String());
    void generate_all_implied_end_tags_thoroughly();

    // Special element checks
    [[nodiscard]] static bool is_special_element(const String& tag_name);
    [[nodiscard]] static bool is_formatting_element(const String& tag_name);

    // Error reporting
    void parse_error(const String& message);

    // Reset insertion mode
    void reset_insertion_mode_appropriately();

    void acknowledge_self_closing_flag() { m_self_closing_flag_acknowledged = true; }

    // Document
    RefPtr<dom::Document> m_document;

    // Parser state
    InsertionMode m_insertion_mode{InsertionMode::Initial};
    InsertionMode m_original_insertion_mode{InsertionMode::Initial};
    std::stack<InsertionMode> m_template_insertion_modes;

    // Stack of open elements
    std::vector<RefPtr<dom::Element>> m_open_elements;

    // Active formatting elements
    std::vector<ActiveFormattingElement> m_active_formatting_elements;

    // Special elements
    dom::Element* m_head_element{nullptr};
    dom::Element* m_form_element{nullptr};
    dom::Element* m_context_element{nullptr};

    // Flags
    bool m_scripting_enabled{false};
    bool m_parser_cannot_change_mode{false};
    bool m_is_iframe_srcdoc{false};
    bool m_frameset_ok{true};
    bool m_foster_parenting{false};
    bool m_self_closing_flag_acknowledged{true};

    // Pending table characters
    std::vector<unicode::CodePoint> m_pending_table_characters;

    // Tokenizer (for state adjustment)
    Tokenizer* m_tokenizer{nullptr};

    // Error callback
    ErrorCallback m_error_callback;
};

} // namespace lithium::html

// Shared helper for SVG camel-casing
namespace lithium::html {
String svg_camel_case(const String& name_lower);
}
