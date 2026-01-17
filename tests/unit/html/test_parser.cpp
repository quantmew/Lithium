#include <gtest/gtest.h>
#include "lithium/html/parser.hpp"
#include "lithium/core/string.hpp"
#include <algorithm>

using namespace lithium;
using namespace lithium::html;

// ============================================================================
// HTML Parser Tests
// ============================================================================

class HTMLParserTest : public ::testing::Test {
protected:
    RefPtr<dom::Document> parse(const String& html) {
        Parser parser;
        return parser.parse(html);
    }
};

TEST_F(HTMLParserTest, EmptyDocument) {
    auto doc = parse("");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->document_element(), nullptr);
}

TEST_F(HTMLParserTest, SimpleDocument) {
    auto doc = parse("<html><head></head><body></body></html>");

    ASSERT_NE(doc, nullptr);
    auto* html = doc->document_element();
    ASSERT_NE(html, nullptr);
    EXPECT_EQ(html->tag_name(), String("html"));
}

TEST_F(HTMLParserTest, ImplicitElements) {
    // HTML parser should create implicit elements
    auto doc = parse("<p>Hello</p>");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->document_element(), nullptr);
    EXPECT_NE(doc->head(), nullptr);
    EXPECT_NE(doc->body(), nullptr);
}

TEST_F(HTMLParserTest, TextContent) {
    auto doc = parse("<p>Hello World</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("Hello World"));
}

TEST_F(HTMLParserTest, NestedElements) {
    auto doc = parse("<div><p><span>Text</span></p></div>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->tag_name(), String("div"));

    auto* p = div->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));

    auto* span = p->first_element_child();
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->tag_name(), String("span"));
}

TEST_F(HTMLParserTest, Attributes) {
    auto doc = parse("<div id=\"test\" class=\"foo bar\"></div>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);

    EXPECT_EQ(div->id(), String("test"));
    EXPECT_EQ(div->class_name(), String("foo bar"));
}

TEST_F(HTMLParserTest, VoidElements) {
    auto doc = parse("<p>Line 1<br>Line 2</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);

    // Should have: text, br, text
    EXPECT_EQ(p->child_nodes().size(), 3u);
}

TEST_F(HTMLParserTest, Doctype) {
    auto doc = parse("<!DOCTYPE html><html></html>");

    ASSERT_NE(doc, nullptr);
    EXPECT_NE(doc->doctype(), nullptr);
    EXPECT_EQ(doc->doctype()->name(), String("html"));
}

TEST_F(HTMLParserTest, Comments) {
    auto doc = parse("<p>Before<!-- comment -->After</p>");

    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);

    // Should have: text, comment, text
    EXPECT_EQ(p->child_nodes().size(), 3u);
}

TEST_F(HTMLParserTest, Script) {
    auto doc = parse("<script>var x = 1 < 2;</script>");

    ASSERT_NE(doc, nullptr);
    // Script content should be preserved
}

TEST_F(HTMLParserTest, Style) {
    auto doc = parse("<style>.foo { color: red; }</style>");

    ASSERT_NE(doc, nullptr);
    // Style content should be preserved
}

TEST_F(HTMLParserTest, GetElementById) {
    auto doc = parse("<div id=\"test\">Hello</div>");

    ASSERT_NE(doc, nullptr);
    auto* element = doc->get_element_by_id(String("test"));
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->tag_name(), String("div"));
}

TEST_F(HTMLParserTest, GetElementsByTagName) {
    auto doc = parse("<p>1</p><p>2</p><div><p>3</p></div>");

    ASSERT_NE(doc, nullptr);
    auto elements = doc->get_elements_by_tag_name(String("p"));
    EXPECT_EQ(elements.size(), 3u);
}

TEST_F(HTMLParserTest, GetElementsByClassName) {
    auto doc = parse("<div class=\"foo\">1</div><p class=\"foo bar\">2</p><span>3</span>");

    ASSERT_NE(doc, nullptr);
    auto elements = doc->get_elements_by_class_name(String("foo"));
    EXPECT_EQ(elements.size(), 2u);
}

TEST_F(HTMLParserTest, ParagraphsAutoCloseWhenStartingNewParagraph) {
    auto doc = parse("<p>one<p>two");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* first_p = body->first_element_child();
    ASSERT_NE(first_p, nullptr);
    EXPECT_EQ(first_p->tag_name(), String("p"));
    EXPECT_EQ(first_p->text_content(), String("one"));

    ASSERT_TRUE(first_p->next_sibling()->is_element());
    auto* second_p = first_p->next_sibling()->as_element();
    ASSERT_NE(second_p, nullptr);
    EXPECT_EQ(second_p->tag_name(), String("p"));
    EXPECT_EQ(second_p->text_content(), String("two"));
}

TEST_F(HTMLParserTest, ListItemsImplicitlyClosePreviousItem) {
    auto doc = parse("<ul><li>one<li>two</ul>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* ul = body->first_element_child();
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->tag_name(), String("ul"));
    ASSERT_EQ(ul->child_nodes().size(), 2u);

    auto* first_li = ul->first_element_child();
    ASSERT_NE(first_li, nullptr);
    EXPECT_EQ(first_li->text_content(), String("one"));

    ASSERT_TRUE(first_li->next_sibling()->is_element());
    auto* second_li = first_li->next_sibling()->as_element();
    ASSERT_NE(second_li, nullptr);
    EXPECT_EQ(second_li->text_content(), String("two"));
}

TEST_F(HTMLParserTest, DescriptionListTermsCloseAutomatically) {
    auto doc = parse("<dl><dt>Term<dd>Def 1<dd>Def 2");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* dl = body->first_element_child();
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->tag_name(), String("dl"));
    ASSERT_EQ(dl->child_nodes().size(), 3u);

    auto* dt = dl->first_element_child();
    ASSERT_NE(dt, nullptr);
    EXPECT_EQ(dt->tag_name(), String("dt"));
    EXPECT_EQ(dt->text_content(), String("Term"));

    ASSERT_TRUE(dt->next_sibling()->is_element());
    auto* dd1 = dt->next_sibling()->as_element();
    ASSERT_NE(dd1, nullptr);
    EXPECT_EQ(dd1->tag_name(), String("dd"));
    EXPECT_EQ(dd1->text_content(), String("Def 1"));

    ASSERT_TRUE(dd1->next_sibling()->is_element());
    auto* dd2 = dd1->next_sibling()->as_element();
    ASSERT_NE(dd2, nullptr);
    EXPECT_EQ(dd2->tag_name(), String("dd"));
    EXPECT_EQ(dd2->text_content(), String("Def 2"));
}

TEST_F(HTMLParserTest, TableCharactersAreFosterParented) {
    auto doc = parse("<div><table>Text<tr><td>Cell</td></tr></table></div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    ASSERT_TRUE(div->first_child()->is_text());
    EXPECT_EQ(div->first_child()->text_content(), String("Text"));

    auto* table = div->first_element_child();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->tag_name(), String("table"));
    // Text should not end up inside the table
    ASSERT_TRUE(table->first_child()->is_element());
    auto* first_in_table = table->first_element_child();
    if (first_in_table->tag_name() == String("tbody")) {
        ASSERT_NE(first_in_table->first_element_child(), nullptr);
        EXPECT_EQ(first_in_table->first_element_child()->tag_name(), String("tr"));
    } else {
        EXPECT_EQ(first_in_table->tag_name(), String("tr"));
    }
}

TEST_F(HTMLParserTest, TemplateContentFosterParentingWithinTable) {
    auto doc = parse("<div><template><table>text<tr><td>cell</td></tr></table></template></div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* container = body->first_element_child();
    ASSERT_NE(container, nullptr);
    auto* tmpl = container->first_element_child();
    ASSERT_NE(tmpl, nullptr);
    ASSERT_TRUE(tmpl->first_child()->is_text());
    EXPECT_EQ(tmpl->first_child()->text_content(), String("text"));
    auto* table = tmpl->first_element_child();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->local_name(), String("table"));
}

TEST_F(HTMLParserTest, CharacterReferencesAreResolved) {
    auto doc = parse("<div title=\"Tom &amp; Jerry\">&lt;span&gt;&#x41;&#65;</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->get_attribute("title"_s).value_or(String()), String("Tom & Jerry"));
    EXPECT_EQ(div->text_content(), String("<span>AA"));
}

TEST_F(HTMLParserTest, RcdataAndRawtextEndTagsAllowTrailingWhitespace) {
    auto doc = parse("<textarea>value</textarea   ><script>var x = 1;</script   ><p>after</p>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);

    auto* textarea = body->first_element_child();
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->tag_name(), String("textarea"));
    EXPECT_EQ(textarea->text_content(), String("value"));

    ASSERT_TRUE(textarea->next_sibling()->is_element());
    auto* script = textarea->next_sibling()->as_element();
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->tag_name(), String("script"));
    EXPECT_EQ(script->text_content(), String("var x = 1;"));

    ASSERT_TRUE(script->next_sibling()->is_element());
    auto* p = script->next_sibling()->as_element();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->tag_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("after"));
}

TEST_F(HTMLParserTest, LegacyNamedReferenceWithoutSemicolonStillDecodes) {
    auto doc = parse("<div>&amp and more</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), String("& and more"));
}

TEST_F(HTMLParserTest, NonLegacyReferenceWithoutSemicolonRejectedWhenFollowedByAlpha) {
    auto doc = parse("<div>&notin members</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->text_content(), String("&notin members"));
}

TEST_F(HTMLParserTest, NumericReferenceControlMappingAndInvalids) {
    auto doc = parse("<div>&#128; &#xD800; &#x110000;</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);

    String expected;
    expected.append(String::from_code_point(0x20AC)); // control mapping 0x80 -> €
    expected.append(" "_s);
    expected.append(String::from_code_point(unicode::REPLACEMENT_CHARACTER));
    expected.append(" "_s);
    expected.append(String::from_code_point(unicode::REPLACEMENT_CHARACTER));

    EXPECT_EQ(div->text_content(), expected);
}

TEST_F(HTMLParserTest, DuplicateAttributesKeepFirst) {
    auto doc = parse("<div id=\"one\" ID=\"two\"></div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->id(), String("one"));
}

TEST_F(HTMLParserTest, DuplicateAttributesReportError) {
    Parser parser;
    auto doc = parser.parse(String("<div id=\"one\" ID=\"two\"></div>"));
    ASSERT_NE(doc, nullptr);

    bool has_duplicate_error = std::any_of(parser.errors().begin(), parser.errors().end(),
        [](const String& err) { return err.find("duplicate"_s).has_value(); });
    EXPECT_TRUE(has_duplicate_error);
}

TEST_F(HTMLParserTest, FormOwnerFromAncestorForm) {
    auto doc = parse("<form id='f'><input name='a'></form>");
    auto forms = doc->get_elements_by_tag_name("form"_s);
    ASSERT_EQ(forms.size(), 1u);
    auto inputs = doc->get_elements_by_tag_name("input"_s);
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs.front()->form_owner(), forms.front());
}

TEST_F(HTMLParserTest, FormAttributeAssociatesControl) {
    auto doc = parse("<form id='f'></form><div><input form='f' name='a'></div>");
    auto forms = doc->get_elements_by_tag_name("form"_s);
    ASSERT_EQ(forms.size(), 1u);
    auto inputs = doc->get_elements_by_tag_name("input"_s);
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs.front()->form_owner(), forms.front());
}

TEST_F(HTMLParserTest, OptionInheritsFormFromSelectFormAttr) {
    auto doc = parse("<form id='f'></form><select form='f'><option>One</option></select>");
    auto forms = doc->get_elements_by_tag_name("form"_s);
    ASSERT_EQ(forms.size(), 1u);
    auto options = doc->get_elements_by_tag_name("option"_s);
    ASSERT_EQ(options.size(), 1u);
    EXPECT_EQ(options.front()->form_owner(), forms.front());
}

TEST_F(HTMLParserTest, TemplateControlsDoNotGainFormOwner) {
    auto doc = parse("<template><input form='f'></template><form id='f'></form>");
    auto templates = doc->get_elements_by_tag_name("template"_s);
    ASSERT_EQ(templates.size(), 1u);
    auto* tmpl = templates.front();
    ASSERT_NE(tmpl, nullptr);
    ASSERT_NE(tmpl->first_element_child(), nullptr);
    auto* input = tmpl->first_element_child();
    EXPECT_EQ(input->form_owner(), nullptr);
}

TEST_F(HTMLParserTest, IsindexCompatibilityCreatesForm) {
    auto doc = parse("<isindex action='/search' prompt='Find:'>");
    auto forms = doc->get_elements_by_tag_name("form"_s);
    ASSERT_EQ(forms.size(), 1u);
    auto inputs = doc->get_elements_by_tag_name("input"_s);
    ASSERT_EQ(inputs.size(), 1u);
    auto* form = forms.front();
    auto* input = inputs.front();
    EXPECT_EQ(input->form_owner(), form);
    EXPECT_EQ(form->get_attribute("action"_s).value_or(String()), String("/search"));
    EXPECT_EQ(input->get_attribute("name"_s).value_or(String()), String("isindex"));
    EXPECT_EQ(input->get_attribute("type"_s).value_or(String()), String("text"));
    auto labels = doc->get_elements_by_tag_name("label"_s);
    ASSERT_EQ(labels.size(), 1u);
    EXPECT_TRUE(labels.front()->text_content().starts_with("Find:"_s));
}

TEST_F(HTMLParserTest, HeadImplicitlyClosedWhenBodyStarts) {
    auto doc = parse("<html><head><meta charset='utf-8'><body><p>hi");
    auto* head = doc->head();
    ASSERT_NE(head, nullptr);
    ASSERT_NE(head->first_element_child(), nullptr);
    EXPECT_EQ(head->first_element_child()->local_name(), String("meta"));
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->first_element_child(), nullptr);
    EXPECT_EQ(body->first_element_child()->local_name(), String("p"));
    EXPECT_EQ(body->first_element_child()->text_content(), String("hi"));
}

TEST_F(HTMLParserTest, TableSectionsImplicitlyCloseWithNewSections) {
    auto doc = parse("<table><thead><tr><td>a<tbody><tr><td>b</table>");
    auto tables = doc->get_elements_by_tag_name("table"_s);
    ASSERT_EQ(tables.size(), 1u);
    auto* table = tables.front();
    ASSERT_NE(table, nullptr);
    auto* thead = table->first_element_child();
    ASSERT_NE(thead, nullptr);
    EXPECT_EQ(thead->local_name(), String("thead"));
    auto* tbody = thead->next_element_sibling();
    ASSERT_NE(tbody, nullptr);
    EXPECT_EQ(tbody->local_name(), String("tbody"));
    EXPECT_EQ(thead->text_content(), String("a"));
    EXPECT_EQ(tbody->text_content(), String("b"));
}

TEST_F(HTMLParserTest, ButtonScopeClosesParagraphInside) {
    auto doc = parse("<button><p>in</button><p>out");
    auto buttons = doc->get_elements_by_tag_name("button"_s);
    ASSERT_EQ(buttons.size(), 1u);
    auto* button = buttons.front();
    ASSERT_NE(button->first_element_child(), nullptr);
    EXPECT_EQ(button->first_element_child()->local_name(), String("p"));
    EXPECT_EQ(button->text_content(), String("in"));
    auto ps = doc->get_elements_by_tag_name("p"_s);
    ASSERT_GE(ps.size(), 2u);
    EXPECT_EQ(ps.back()->text_content(), String("out"));
}

TEST_F(HTMLParserTest, SelfClosingNonVoidRaisesErrorButInserts) {
    Parser parser;
    auto doc = parser.parse(String("<div/>"));
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->local_name(), String("div"));
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, SelfClosingVoidIsAcknowledged) {
    Parser parser;
    auto doc = parser.parse(String("<div><br/></div>"));
    ASSERT_NE(doc, nullptr);
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->first_element_child(), nullptr);
    EXPECT_TRUE(parser.errors().empty());
}

TEST_F(HTMLParserTest, MultipleNonVoidSelfClosingAccumulatesErrors) {
    Parser parser;
    auto doc = parser.parse(String("<div/><p/>"));
    auto divs = doc->get_elements_by_tag_name("div"_s);
    auto ps = doc->get_elements_by_tag_name("p"_s);
    EXPECT_EQ(divs.size(), 1u);
    EXPECT_EQ(ps.size(), 1u);
    EXPECT_GE(parser.errors().size(), 2u);
}

TEST_F(HTMLParserTest, QuirksAndLimitedQuirksFromDoctype) {
    Parser parser1;
    auto doc1 = parser1.parse(String("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html></html>"));
    EXPECT_EQ(doc1->quirks_mode(), dom::Document::QuirksMode::Quirks);

    Parser parser2;
    auto doc2 = parser2.parse(String("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\"><html></html>"));
    EXPECT_EQ(doc2->quirks_mode(), dom::Document::QuirksMode::LimitedQuirks);

    Parser parser3;
    auto doc3 = parser3.parse(String("<!DOCTYPE html><html></html>"));
    EXPECT_EQ(doc3->quirks_mode(), dom::Document::QuirksMode::NoQuirks);
}
TEST_F(HTMLParserTest, SelectOptionAutoCloseAndOptgroup) {
    auto doc = parse("<select><option>One<option>Two<optgroup label=\"g\"><option>Three</optgroup><option>Four</select>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* select = body->first_element_child();
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->local_name(), String("select"));

    std::vector<String> values;
    for (auto* child = select->first_element_child(); child; child = child->next_element_sibling()) {
        if (child->local_name() == "option"_s) {
            values.push_back(child->text_content());
        } else if (child->local_name() == "optgroup"_s) {
            auto* opt = child->first_element_child();
            if (opt) values.push_back(opt->text_content());
        }
    }
    ASSERT_EQ(values.size(), 4u);
    EXPECT_EQ(values[0], String("One"));
    EXPECT_EQ(values[1], String("Two"));
    EXPECT_EQ(values[2], String("Three"));
    EXPECT_EQ(values[3], String("Four"));
}

TEST_F(HTMLParserTest, SelectEndsWhenTextareaAppears) {
    auto doc = parse("<select><option>One<textarea>txt</textarea>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* select = body->first_element_child();
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->local_name(), String("select"));
    ASSERT_EQ(select->child_element_count(), 1u);

    auto* textarea = select->next_element_sibling();
    ASSERT_NE(textarea, nullptr);
    EXPECT_EQ(textarea->local_name(), String("textarea"));
}

TEST_F(HTMLParserTest, TemplateInsertionModeSwitch) {
    auto doc = parse("<template><select><option>One</template><p>After</p>");

    auto templates = doc->get_elements_by_tag_name("template"_s);
    ASSERT_FALSE(templates.empty());
    auto* tmpl = templates.front();
    ASSERT_NE(tmpl, nullptr);
    ASSERT_NE(tmpl->first_element_child(), nullptr);
    EXPECT_EQ(tmpl->first_element_child()->local_name(), String("select"));

    auto ps = doc->get_elements_by_tag_name("p"_s);
    ASSERT_EQ(ps.size(), 1u);
    auto* p = ps.front();
    ASSERT_NE(p, nullptr);
    auto* parent = p->parent_node()->as_element();
    ASSERT_NE(parent, nullptr);
    EXPECT_NE(parent->local_name(), String("template"));
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("After"));
}

TEST_F(HTMLParserTest, FramesetParsing) {
    auto doc = parse("<frameset><frame src=\"a\"><frameset><frame src=\"b\"></frameset></frameset>");

    auto* html = doc->document_element();
    ASSERT_NE(html, nullptr);
    auto* frameset = html->first_element_child();
    while (frameset && frameset->local_name() == "head"_s) {
        frameset = frameset->next_element_sibling();
    }
    ASSERT_NE(frameset, nullptr);
    EXPECT_EQ(frameset->local_name(), String("frameset"));
    ASSERT_NE(frameset->first_element_child(), nullptr);
    EXPECT_EQ(frameset->first_element_child()->local_name(), String("frame"));
}

TEST_F(HTMLParserTest, PlaintextTreatsMarkupAsText) {
    auto doc = parse("<plaintext>Hello<div>not a tag</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* plaintext = body->first_element_child();
    ASSERT_NE(plaintext, nullptr);
    EXPECT_EQ(plaintext->local_name(), String("plaintext"));
    ASSERT_TRUE(plaintext->first_child()->is_text());
    EXPECT_EQ(plaintext->child_nodes().size(), 1u);
    EXPECT_EQ(plaintext->text_content(), String("Hello<div>not a tag</div>"));
}

TEST_F(HTMLParserTest, CaptionContentDoesNotWrapTableRows) {
    auto doc = parse("<table><caption>Title<tr><td>Cell</td></tr></table>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* table = body->first_element_child();
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->local_name(), String("table"));

    auto* caption = table->first_element_child();
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->local_name(), String("caption"));
    EXPECT_EQ(caption->text_content(), String("Title"));
    if (caption->first_element_child()) {
        EXPECT_NE(caption->first_element_child()->local_name(), String("tr"));
    }

    auto* after_caption = caption->next_element_sibling();
    ASSERT_NE(after_caption, nullptr);
    EXPECT_TRUE(after_caption->local_name() == String("tbody") ||
                after_caption->local_name() == String("tr"));
}

TEST_F(HTMLParserTest, ColTagsImplyColgroup) {
    auto doc = parse("<table><col span=\"2\"><tr><td>Cell</td></tr></table>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* table = body->first_element_child();
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->local_name(), String("table"));

    auto* first_child = table->first_element_child();
    ASSERT_NE(first_child, nullptr);
    EXPECT_EQ(first_child->local_name(), String("colgroup"));
    auto* col = first_child->first_element_child();
    ASSERT_NE(col, nullptr);
    EXPECT_EQ(col->local_name(), String("col"));
    EXPECT_EQ(col->get_attribute("span"_s).value_or(String()), String("2"));
}

TEST_F(HTMLParserTest, ParseFragmentWithContextElement) {
    Parser parser;
    auto context_doc = make_ref<dom::Document>();
    auto context = context_doc->create_element("div"_s);

    auto fragment = parser.parse_fragment("<p>Hello <b>world</b>", context.get());
    ASSERT_NE(fragment, nullptr);
    ASSERT_EQ(fragment->child_nodes().size(), 1u);

    auto* p = fragment->child_nodes()[0]->as_element();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("Hello world"));
}

TEST_F(HTMLParserTest, FragmentRespectsTableContext) {
    Parser parser;
    auto context_doc = make_ref<dom::Document>();
    auto table_context = context_doc->create_element("table"_s);

    auto fragment = parser.parse_fragment("<tr><td>Cell</td></tr>", table_context.get());
    ASSERT_NE(fragment, nullptr);
    ASSERT_GE(fragment->child_nodes().size(), 1u);

    auto* first = fragment->child_nodes()[0]->as_element();
    ASSERT_NE(first, nullptr);
    if (first->local_name() == String("tbody")) {
        first = first->first_element_child();
    }
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->local_name(), String("tr"));
    auto* cell = first->first_element_child();
    ASSERT_NE(cell, nullptr);
    EXPECT_EQ(cell->local_name(), String("td"));
    EXPECT_EQ(cell->text_content(), String("Cell"));
}

TEST_F(HTMLParserTest, SvgNamespaceAndIntegrationPoint) {
    auto doc = parse("<svg><lineargradient id='g'></lineargradient><foreignObject><p>hi</p></foreignObject></svg>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* svg = body->first_element_child();
    ASSERT_NE(svg, nullptr);
    EXPECT_EQ(svg->namespace_uri(), String("http://www.w3.org/2000/svg"));
    EXPECT_EQ(svg->local_name(), String("svg"));

    auto* gradient = svg->first_element_child();
    ASSERT_NE(gradient, nullptr);
    EXPECT_EQ(gradient->local_name(), String("linearGradient"));
    EXPECT_EQ(gradient->namespace_uri(), String("http://www.w3.org/2000/svg"));

    auto* foreign = gradient->next_element_sibling();
    ASSERT_NE(foreign, nullptr);
    EXPECT_EQ(foreign->local_name(), String("foreignObject"));
    ASSERT_NE(foreign->first_element_child(), nullptr);
    auto* p = foreign->first_element_child();
    EXPECT_EQ(p->namespace_uri(), String()); // HTML namespace
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("hi"));
}

TEST_F(HTMLParserTest, MathmlNamespaceAndAnnotationIntegration) {
    auto doc = parse("<math><mi>x</mi><annotation-xml encoding='application/xhtml+xml'><p>math html</p></annotation-xml></math>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* math = body->first_element_child();
    ASSERT_NE(math, nullptr);
    EXPECT_EQ(math->namespace_uri(), String("http://www.w3.org/1998/Math/MathML"));
    EXPECT_EQ(math->local_name(), String("math"));

    auto* mi = math->first_element_child();
    ASSERT_NE(mi, nullptr);
    EXPECT_EQ(mi->namespace_uri(), String("http://www.w3.org/1998/Math/MathML"));
    EXPECT_EQ(mi->local_name(), String("mi"));

    auto* annotation = mi->next_element_sibling();
    ASSERT_NE(annotation, nullptr);
    EXPECT_EQ(annotation->local_name(), String("annotation-xml"));

    auto* p = annotation->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->namespace_uri(), String());
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("math html"));
}

TEST_F(HTMLParserTest, CharacterReferencesFullTableAndBoundaries) {
    auto doc = parse("<div>&NotEqualTilde; &#0; &#x10FFFF; &Aacute and &Aacute1</div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);

    String expected;
    expected.append(String::from_code_point(0x2242));
    expected.append(String::from_code_point(0x0338));
    expected.append(" "_s);
    expected.append(String::from_code_point(unicode::REPLACEMENT_CHARACTER));
    expected.append(" "_s);
    expected.append(String::from_code_point(0x10FFFF));
    expected.append(" "_s);
    expected.append(String::from_code_point(0x00C1));
    expected.append(" and "_s);
    expected.append("&Aacute1"_s);

    EXPECT_EQ(div->text_content(), expected);
}

TEST_F(HTMLParserTest, ScriptEndTagAttributesAndCaseInsensitive) {
    auto doc = parse("<script><!-- test --></SCRIPT data-x=\"1\"><p>after</p>");

    auto* head = doc->head();
    ASSERT_NE(head, nullptr);
    auto* script = head->first_element_child();
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->local_name(), String("script"));
    EXPECT_EQ(script->text_content(), String("<!-- test -->"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("after"));
}

TEST_F(HTMLParserTest, ScriptDoubleEscapedKeepsSingleScript) {
    Parser parser;
    auto doc = parser.parse(String("<script><!--<script></script>--></script>"));

    auto scripts = doc->get_elements_by_tag_name("script"_s);
    ASSERT_EQ(scripts.size(), 1u);
    auto* script = scripts.front();
    ASSERT_NE(script, nullptr);
    EXPECT_FALSE(script->text_content().empty());
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, ScriptCallbackInjectsDocumentWriteContent) {
    Parser parser;
    String captured;
    parser.set_script_callback([&](const String& text) {
        captured = text;
        parser.write("<p>written</p>");
    });
    parser.begin();
    parser.write("<html><body><script>hello()</script><div>after</div>");
    auto doc = parser.finish();

    EXPECT_EQ(captured, String("hello()"));
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* script = body->first_element_child();
    ASSERT_NE(script, nullptr);
    EXPECT_EQ(script->local_name(), String("script"));
    auto* div = script->next_element_sibling();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->local_name(), String("div"));
    EXPECT_EQ(div->text_content(), String("after"));
    auto* p = div->next_element_sibling();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("written"));
}

TEST_F(HTMLParserTest, SelfClosingHeadRaisesError) {
    Parser parser;
    auto doc = parser.parse(String("<html><head/></html>"));
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, FosterParentingTemplateSiblingInsideTable) {
    auto doc = parse("<div><table><template></template>text<tr><td>cell</td></tr></table></div>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* container = body->first_element_child();
    ASSERT_NE(container, nullptr);
    auto* first = container->first_child();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(first->is_element());
    auto* template_el = first->as_element();
    ASSERT_NE(template_el, nullptr);
    auto* text_node = template_el->next_sibling();
    ASSERT_TRUE(text_node && text_node->is_text());
    EXPECT_EQ(text_node->text_content(), String("text"));
    auto* table = text_node->next_sibling()->as_element();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->local_name(), String("table"));
}

TEST_F(HTMLParserTest, SelectOptgroupAndOptionScopeClosing) {
    auto doc = parse("<select><optgroup label='g'><option>One</select><p>after</p>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* select = body->first_element_child();
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->local_name(), String("select"));

    auto* optgroup = select->first_element_child();
    ASSERT_NE(optgroup, nullptr);
    EXPECT_EQ(optgroup->local_name(), String("optgroup"));
    auto* option = optgroup->first_element_child();
    ASSERT_NE(option, nullptr);
    EXPECT_EQ(option->local_name(), String("option"));
    EXPECT_EQ(option->text_content(), String("One"));

    auto* p = select->next_element_sibling();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("after"));
}

TEST_F(HTMLParserTest, AfterFramesetTextIgnored) {
    Parser parser;
    auto doc = parser.parse(String("<frameset></frameset>text"));

    auto* html = doc->document_element();
    ASSERT_NE(html, nullptr);
    dom::Element* frameset = nullptr;
    for (auto* child = html->first_element_child(); child; child = child->next_element_sibling()) {
        if (child->local_name() == "frameset"_s) {
            frameset = child;
            break;
        }
    }
    ASSERT_NE(frameset, nullptr);
    EXPECT_EQ(doc->body(), nullptr);
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, AdoptionAgencyRepairsMisnestedFormatting) {
    Parser parser;
    auto doc = parser.parse(String("<p><b><i></b>text</i></p>"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    auto* b = p->first_element_child();
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->local_name(), String("b"));
    auto* i = b->first_element_child();
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->local_name(), String("i"));
    EXPECT_EQ(i->text_content(), String("text"));
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, AdoptionAgencyHandlesDeeperMismatches) {
    auto doc = parse("<p><b><i>1</b>2</i>");
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    auto* first = p->first_element_child();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->local_name(), String("b"));
    ASSERT_NE(first->first_element_child(), nullptr);
    EXPECT_EQ(first->first_element_child()->local_name(), String("i"));
    EXPECT_EQ(first->first_element_child()->text_content(), String("12"));
}

TEST_F(HTMLParserTest, ActiveFormattingElementListPrunesOldEntries) {
    auto doc = parse("<p><b id='1'><b id='2'><b id='3'><b id='4'>x</b></b></b></b>");
    auto bs = doc->get_elements_by_tag_name("b"_s);
    ASSERT_EQ(bs.size(), 4u);
}

TEST_F(HTMLParserTest, ForeignObjectThenSvgKeepsNamespaces) {
    auto doc = parse("<svg><foreignObject><p>html</p></foreignObject><g><title>t</title></g></svg><p>after</p>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* svg = body->first_element_child();
    ASSERT_NE(svg, nullptr);
    EXPECT_EQ(svg->namespace_uri(), String("http://www.w3.org/2000/svg"));

    auto* foreign = svg->first_element_child();
    ASSERT_NE(foreign, nullptr);
    EXPECT_EQ(foreign->local_name(), String("foreignObject"));
    auto* p_html = foreign->first_element_child();
    ASSERT_NE(p_html, nullptr);
    EXPECT_EQ(p_html->namespace_uri(), String());
    EXPECT_EQ(p_html->text_content(), String("html"));

    auto* group = foreign->next_element_sibling();
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->namespace_uri(), String("http://www.w3.org/2000/svg"));
    auto* title = group->first_element_child();
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->namespace_uri(), String("http://www.w3.org/2000/svg"));
    EXPECT_EQ(title->text_content(), String("t"));

    auto* after = svg->next_element_sibling();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->namespace_uri(), String());
    EXPECT_EQ(after->local_name(), String("p"));
    EXPECT_EQ(after->text_content(), String("after"));
}

TEST_F(HTMLParserTest, SvgAttributesAdjustedAndPrefixesHandled) {
    auto doc = parse("<svg viewbox='0 0 1 1' preserveaspectratio='xMidYMid meet' xlink:href='one' xml:lang='en' xmlns:custom='u'><lineargradient id='g'></lineargradient></svg>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* svg = body->first_element_child();
    ASSERT_NE(svg, nullptr);
    EXPECT_EQ(svg->namespace_uri(), String("http://www.w3.org/2000/svg"));
    auto attrs = svg->attributes();
    bool has_viewbox = false;
    bool has_preserve = false;
    for (const auto& attr : attrs) {
        if (attr.name == "viewBox"_s) has_viewbox = true;
        if (attr.name == "preserveAspectRatio"_s) has_preserve = true;
    }
    EXPECT_TRUE(has_viewbox);
    EXPECT_TRUE(has_preserve);
    EXPECT_EQ(svg->get_attribute("viewBox"_s).value_or(String()), String("0 0 1 1"));
    EXPECT_EQ(svg->get_attribute("xlink:href"_s).value_or(String()), String("one"));
    EXPECT_EQ(svg->get_attribute("xml:lang"_s).value_or(String()), String("en"));
    EXPECT_EQ(svg->get_attribute("xmlns:custom"_s).value_or(String()), String("u"));

    auto* gradient = svg->first_element_child();
    ASSERT_NE(gradient, nullptr);
    EXPECT_EQ(gradient->local_name(), String("linearGradient"));
}

TEST_F(HTMLParserTest, MathmlAttributeAdjusted) {
    auto doc = parse("<math><mi definitionurl='foo'></mi></math>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* math = body->first_element_child();
    ASSERT_NE(math, nullptr);
    auto* mi = math->first_element_child();
    ASSERT_NE(mi, nullptr);
    EXPECT_EQ(mi->get_attribute("definitionURL"_s).value_or(String()), String("foo"));
}

TEST_F(HTMLParserTest, MathmlTextIntegrationFallsBackToHtml) {
    auto doc = parse("<math><mtext><span id='x'>hi</span></mtext><mtext><mglyph></mglyph></mtext></math>");

    auto spans = doc->get_elements_by_tag_name("span"_s);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans.front()->namespace_uri(), String());

    auto mglyphs = doc->get_elements_by_tag_name("mglyph"_s);
    ASSERT_EQ(mglyphs.size(), 1u);
    EXPECT_EQ(mglyphs.front()->namespace_uri(), String("http://www.w3.org/1998/Math/MathML"));
}

TEST_F(HTMLParserTest, AnnotationXmlHtmlIntegrationByEncoding) {
    auto doc = parse("<math><annotation-xml encoding='application/xhtml+xml'><p>html</p></annotation-xml><annotation-xml encoding='text/html'><div>also html</div></annotation-xml></math>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* math = body->first_element_child();
    ASSERT_NE(math, nullptr);
    auto* first_ann = math->first_element_child();
    ASSERT_NE(first_ann, nullptr);
    EXPECT_EQ(first_ann->namespace_uri(), String("http://www.w3.org/1998/Math/MathML"));
    auto* p = first_ann->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->namespace_uri(), String());
    EXPECT_EQ(p->local_name(), String("p"));

    auto* second_ann = first_ann->next_element_sibling();
    ASSERT_NE(second_ann, nullptr);
    auto* div = second_ann->first_element_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->namespace_uri(), String());
    EXPECT_EQ(div->local_name(), String("div"));
    EXPECT_EQ(div->text_content(), String("also html"));
}

TEST_F(HTMLParserTest, AdditionalQuirksTriggers) {
    Parser parser1;
    auto doc1 = parser1.parse(String("<!DOCTYPE html SYSTEM \"http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd\"><html></html>"));
    EXPECT_EQ(doc1->quirks_mode(), dom::Document::QuirksMode::Quirks);

    Parser parser2;
    auto doc2 = parser2.parse(String("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Frameset//EN\" \"http://www.w3.org/TR/html4/frameset.dtd\"><html></html>"));
    EXPECT_EQ(doc2->quirks_mode(), dom::Document::QuirksMode::LimitedQuirks);

    Parser parser3;
    auto doc3 = parser3.parse(String("<!DOCTYPE><html></html>"));
    EXPECT_EQ(doc3->quirks_mode(), dom::Document::QuirksMode::Quirks);
}

TEST_F(HTMLParserTest, VoidSelfClosingAcknowledged) {
    Parser parser;
    auto doc = parser.parse(String("<br/><img src='x'/>"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->child_element_count(), 2u);
    EXPECT_EQ(body->first_element_child()->local_name(), String("br"));
    EXPECT_EQ(body->first_element_child()->next_element_sibling()->local_name(), String("img"));
    EXPECT_TRUE(parser.errors().empty());
}

TEST_F(HTMLParserTest, Utf8BomIsIgnored) {
    Parser parser;
    auto doc = parser.parse(String("\xEF\xBB\xBF<html><body><p>hi</p></body></html>"));
    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), String("hi"));
    EXPECT_TRUE(parser.errors().empty());
}

TEST_F(HTMLParserTest, MetaCharsetUtf8Accepted) {
    Parser parser;
    auto doc = parser.parse(String("<meta charset=\"UTF-8\"><p>ok</p>"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), String("ok"));
    EXPECT_TRUE(parser.errors().empty());
}

TEST_F(HTMLParserTest, UnsupportedMetaCharsetReportsError) {
    Parser parser;
    auto doc = parser.parse(String("<meta charset=\"windows-1252\"><p>&#233;</p>"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    String expected;
    expected.append(String::from_code_point(233));
    EXPECT_EQ(p->text_content(), expected);
    EXPECT_FALSE(parser.errors().empty());
}

TEST_F(HTMLParserTest, UnsupportedMetaCharsetInBodyReportsErrorAndParses) {
    Parser parser;
    auto doc = parser.parse(String("<html><head></head><body><meta charset='shift_jis'><p>hi</p></body></html>"));
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(parser.errors().empty());
    auto ps = doc->get_elements_by_tag_name("p"_s);
    ASSERT_EQ(ps.size(), 1u);
    EXPECT_EQ(ps.front()->text_content(), String("hi"));
}

TEST_F(HTMLParserTest, StreamingUnsupportedCharsetTriggersError) {
    Parser parser;
    parser.begin();
    parser.write("<div>");
    parser.write("<meta charset='windows-1252'><p>ok</p>");
    auto doc = parser.finish();
    ASSERT_NE(doc, nullptr);
    EXPECT_FALSE(parser.errors().empty());
    auto ps = doc->get_elements_by_tag_name("p"_s);
    ASSERT_EQ(ps.size(), 1u);
    EXPECT_EQ(ps.front()->text_content(), String("ok"));
}

TEST_F(HTMLParserTest, DocumentWriteStreamingBuildsSingleTree) {
    Parser parser;
    parser.begin();
    parser.write("<html><body><div>");
    parser.write("<span>hello");
    parser.write("</span>");
    parser.write("</div>");
    auto doc = parser.finish();

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_element_child();
    ASSERT_NE(div, nullptr);
    auto* span = div->first_element_child();
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text_content(), String("hello"));
    EXPECT_TRUE(parser.errors().empty());
}

TEST_F(HTMLParserTest, DocumentWriteAcrossOpenElements) {
    Parser parser;
    parser.begin();
    parser.write("<p>first ");
    parser.write("second");
    parser.write("</p>");
    auto doc = parser.finish();

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), String("first second"));
}

TEST_F(HTMLParserTest, NoscriptHeadWhenScriptingDisabledUsesHeadRules) {
    Parser parser;
    parser.set_scripting_enabled(false);
    auto doc = parser.parse(String("<head><noscript><link rel='stylesheet' href='x'></noscript></head><body><p>t</p></body>"));

    auto* head = doc->head();
    ASSERT_NE(head, nullptr);
    auto* noscript = head->first_element_child();
    ASSERT_NE(noscript, nullptr);
    EXPECT_EQ(noscript->local_name(), String("noscript"));
    auto* link = noscript->first_element_child();
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(link->local_name(), String("link"));

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* p = body->first_element_child();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->text_content(), String("t"));
}

TEST_F(HTMLParserTest, NoscriptHeadWhenScriptingEnabledRawtext) {
    Parser parser;
    parser.set_scripting_enabled(true);
    auto doc = parser.parse(String("<head><noscript><style></style></noscript></head><body></body>"));

    auto* head = doc->head();
    ASSERT_NE(head, nullptr);
    auto* noscript = head->first_element_child();
    ASSERT_NE(noscript, nullptr);
    EXPECT_EQ(noscript->local_name(), String("noscript"));
    ASSERT_TRUE(noscript->first_child()->is_text());
    EXPECT_EQ(noscript->first_child()->text_content(), String("<style></style>"));
}

TEST_F(HTMLParserTest, IframeSrcdocSkipsLimitedQuirks) {
    Parser parser;
    parser.set_iframe_srcdoc(true);
    auto doc = parser.parse(String("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\"><html></html>"));
    EXPECT_EQ(doc->quirks_mode(), dom::Document::QuirksMode::NoQuirks);
}

TEST_F(HTMLParserTest, ParserCannotChangeModeSkipsLimitedQuirks) {
    Parser parser;
    parser.set_parser_cannot_change_mode(true);
    auto doc = parser.parse(String("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\"><html></html>"));
    EXPECT_EQ(doc->quirks_mode(), dom::Document::QuirksMode::NoQuirks);
}

TEST_F(HTMLParserTest, TableSectionImplicitClosure) {
    auto doc = parse("<table><tbody><tr><td>1</td></tr><tbody><tr><td>2</td></tr></table>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* table = body->first_element_child();
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->local_name(), String("table"));
    auto* first_section = table->first_element_child();
    ASSERT_NE(first_section, nullptr);
    ASSERT_EQ(first_section->local_name(), String("tbody"));
    auto* second_section = first_section->next_element_sibling();
    ASSERT_NE(second_section, nullptr);
    EXPECT_EQ(second_section->local_name(), String("tbody"));
}

TEST_F(HTMLParserTest, TableCellClosedOnTableEnd) {
    auto doc = parse("<table><tr><td>hi</table><p>after</p>");

    auto* body = doc->body();
    ASSERT_NE(body, nullptr);
    auto* outer_table = body->first_element_child();
    ASSERT_NE(outer_table, nullptr);
    auto* after = outer_table->next_element_sibling();
    ASSERT_NE(after, nullptr);
    auto* p = after->as_element();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->local_name(), String("p"));
    EXPECT_EQ(p->text_content(), String("after"));
}
