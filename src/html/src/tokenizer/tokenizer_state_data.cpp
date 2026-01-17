/**
 * HTML Tokenizer - data, raw text, script, and CDATA states
 */

#include "lithium/html/tokenizer.hpp"
#include <cctype>

namespace lithium::html {

void Tokenizer::handle_data_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '&') {
        m_return_state = TokenizerState::Data;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == '<') {
        m_state = TokenizerState::TagOpen;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(*cp);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_rcdata_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '&') {
        m_return_state = TokenizerState::RCDATA;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == '<') {
        m_state = TokenizerState::RCDATALessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_rawtext_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '<') {
        m_state = TokenizerState::RAWTEXTLessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_script_data_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '<') {
        m_state = TokenizerState::ScriptDataLessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_plaintext_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_rcdata_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::RCDATAEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::RCDATA;
}

void Tokenizer::handle_rcdata_end_tag_open_state() {
    auto cp = peek();
    if (cp && std::isalpha(*cp)) {
        m_current_token = TagToken{};
        std::get<TagToken>(*m_current_token).is_end_tag = true;
        m_temp_buffer.clear();
        m_state = TokenizerState::RCDATAEndTagName;
        return;
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::RCDATA;
}

void Tokenizer::handle_rcdata_end_tag_name_state() {
    auto flush_as_text = [&] {
        emit_character('<');
        emit_character('/');
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            emit_character(ch);
        }
    };

    auto cp = peek();
    if (!cp) {
        flush_as_text();
        emit_eof();
        return;
    }

    if (std::isalpha(*cp)) {
        consume();
        m_temp_buffer.append(static_cast<char>(std::tolower(*cp)));
        return;
    }

    auto tag_name = m_temp_buffer.build();
    bool is_appropriate = tag_name == m_last_start_tag_name;

    if (is_appropriate && (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ')) {
        m_state = TokenizerState::BeforeAttributeName;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '>') {
        consume();
        std::get<TagToken>(*m_current_token).name = tag_name;
        emit_current_token();
        m_state = TokenizerState::Data;
        return;
    }

    flush_as_text();
    m_state = TokenizerState::RCDATA;
}

void Tokenizer::handle_rawtext_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::RAWTEXTEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::RAWTEXT;
}

void Tokenizer::handle_rawtext_end_tag_open_state() {
    auto cp = peek();
    if (cp && std::isalpha(*cp)) {
        m_current_token = TagToken{};
        std::get<TagToken>(*m_current_token).is_end_tag = true;
        m_temp_buffer.clear();
        m_state = TokenizerState::RAWTEXTEndTagName;
        return;
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::RAWTEXT;
}

void Tokenizer::handle_rawtext_end_tag_name_state() {
    auto flush_as_text = [&] {
        emit_character('<');
        emit_character('/');
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            emit_character(ch);
        }
    };

    auto cp = peek();
    if (!cp) {
        flush_as_text();
        emit_eof();
        return;
    }

    if (std::isalpha(*cp)) {
        consume();
        m_temp_buffer.append(static_cast<char>(std::tolower(*cp)));
        return;
    }

    auto tag_name = m_temp_buffer.build();
    bool is_appropriate = tag_name == m_last_start_tag_name;

    if (is_appropriate && (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ')) {
        m_state = TokenizerState::BeforeAttributeName;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '>') {
        consume();
        std::get<TagToken>(*m_current_token).name = tag_name;
        emit_current_token();
        m_state = TokenizerState::Data;
        return;
    }

    flush_as_text();
    m_state = TokenizerState::RAWTEXT;
}

void Tokenizer::handle_script_data_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::ScriptDataEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_end_tag_open_state() {
    auto cp = peek();
    if (cp && std::isalpha(*cp)) {
        m_current_token = TagToken{};
        std::get<TagToken>(*m_current_token).is_end_tag = true;
        m_temp_buffer.clear();
        m_state = TokenizerState::ScriptDataEndTagName;
        return;
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_end_tag_name_state() {
    auto flush_as_text = [&] {
        emit_character('<');
        emit_character('/');
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            emit_character(ch);
        }
    };

    auto cp = peek();
    if (!cp) {
        flush_as_text();
        emit_eof();
        return;
    }

    if (std::isalpha(*cp)) {
        consume();
        m_temp_buffer.append(static_cast<char>(std::tolower(*cp)));
        return;
    }

    auto tag_name = m_temp_buffer.build();
    bool is_appropriate = tag_name == m_last_start_tag_name;

    if (is_appropriate && (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ')) {
        m_state = TokenizerState::BeforeAttributeName;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
        std::get<TagToken>(*m_current_token).name = tag_name;
        consume();
        return;
    }

    if (is_appropriate && *cp == '>') {
        consume();
        std::get<TagToken>(*m_current_token).name = tag_name;
        emit_current_token();
        m_state = TokenizerState::Data;
        return;
    }

    flush_as_text();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escape_start_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escape_start_dash_state() {
    emit_character('-');
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_dash_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_less_than_sign_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_end_tag_open_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_escaped_end_tag_name_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escape_start_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escaped_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escaped_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escaped_dash_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escaped_less_than_sign_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_script_data_double_escape_end_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

void Tokenizer::handle_cdata_section_state() {
    emit_character(consume());
}

void Tokenizer::handle_cdata_section_bracket_state() {
    emit_character(']');
}

void Tokenizer::handle_cdata_section_end_state() {
    emit_character(']');
    emit_character(']');
}

} // namespace lithium::html
