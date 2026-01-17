/**
 * HTML Tokenizer - comment and DOCTYPE states
 */

#include "lithium/html/tokenizer.hpp"
#include <cctype>

namespace lithium::html {

void Tokenizer::handle_bogus_comment_state() {
    auto cp = peek();
    if (!cp) {
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        comment.data = comment.data + String(1, static_cast<char>(0xFFFD));
    } else {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_markup_declaration_open_state() {
    if (consume_if_match("--", false)) {
        m_current_token = CommentToken{};
        m_state = TokenizerState::CommentStart;
    } else if (consume_if_match("DOCTYPE", true)) {
        m_state = TokenizerState::DOCTYPE;
    } else if (consume_if_match("[CDATA[", false)) {
        if (m_in_foreign_content) {
            m_state = TokenizerState::CDATASection;
        } else {
            parse_error("cdata-in-html-content"_s);
            m_current_token = CommentToken{};
            std::get<CommentToken>(*m_current_token).data = "[CDATA["_s;
            m_state = TokenizerState::BogusComment;
        }
    } else {
        parse_error("incorrectly-opened-comment"_s);
        m_current_token = CommentToken{};
        m_state = TokenizerState::BogusComment;
    }
}

void Tokenizer::handle_comment_start_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentStartDash;
    } else if (*cp == '>') {
        parse_error("abrupt-closing-of-empty-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '<') {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
        m_state = TokenizerState::CommentLessThanSign;
    } else if (*cp == '-') {
        m_state = TokenizerState::CommentEndDash;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        comment.data = comment.data + String(1, static_cast<char>(0xFFFD));
    } else {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_comment_start_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentEnd;
    } else if (*cp == '>') {
        parse_error("abrupt-closing-of-empty-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "-"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '!') {
        m_state = TokenizerState::CommentLessThanSignBang;
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_less_than_sign_bang_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentLessThanSignBangDash;
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_less_than_sign_bang_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentLessThanSignBangDashDash;
    } else {
        reconsume();
        m_state = TokenizerState::CommentEndDash;
    }
}

void Tokenizer::handle_comment_less_than_sign_bang_dash_dash_state() {
    auto cp = peek();
    if (!cp || *cp == '>') {
        reconsume();
        m_state = TokenizerState::CommentEnd;
    } else {
        parse_error("nested-comment"_s);
        reconsume();
        m_state = TokenizerState::CommentEnd;
    }
}

void Tokenizer::handle_comment_end_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentEnd;
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "-"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_end_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '!') {
        m_state = TokenizerState::CommentEndBang;
    } else if (*cp == '-') {
        comment.data = comment.data + "-"_s;
    } else {
        comment.data = comment.data + "--"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_end_bang_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "--!"_s;
        m_state = TokenizerState::CommentEndDash;
    } else if (*cp == '>') {
        parse_error("incorrectly-closed-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "--!"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_doctype_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPEName;
    } else if (*cp == '>') {
        reconsume();
        m_state = TokenizerState::BeforeDOCTYPEName;
    } else {
        parse_error("missing-whitespace-before-doctype-name"_s);
        reconsume();
        m_state = TokenizerState::BeforeDOCTYPEName;
    }
}

void Tokenizer::handle_before_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
    } else if (std::isupper(*cp)) {
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(std::tolower(*cp)));
        m_state = TokenizerState::DOCTYPEName;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(0xFFFD));
        m_state = TokenizerState::DOCTYPEName;
    } else if (*cp == '>') {
        parse_error("missing-doctype-name"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(*cp));
        m_state = TokenizerState::DOCTYPEName;
    }
}

void Tokenizer::handle_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& doctype = std::get<DoctypeToken>(*m_current_token);

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::AfterDOCTYPEName;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (std::isupper(*cp)) {
        doctype.name = doctype.name + String(1, static_cast<char>(std::tolower(*cp)));
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        doctype.name = doctype.name + String(1, static_cast<char>(0xFFFD));
    } else {
        doctype.name = doctype.name + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_after_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // stay in this state
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if ((*cp == 'p' || *cp == 'P') && consume_if_match("ublic", true)) {
        m_state = TokenizerState::AfterDOCTYPEPublicKeyword;
    } else if ((*cp == 's' || *cp == 'S') && consume_if_match("ystem", true)) {
        m_state = TokenizerState::AfterDOCTYPESystemKeyword;
    } else {
        parse_error("invalid-character-sequence-after-doctype-name"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_after_doctype_public_keyword_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPEPublicIdentifier;
    } else if (*cp == '"') {
        parse_error("missing-whitespace-after-doctype-public-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-after-doctype-public-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_before_doctype_public_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_doctype_public_identifier_double_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '"') {
        m_state = TokenizerState::AfterDOCTYPEPublicIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_doctype_public_identifier_single_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\'') {
        m_state = TokenizerState::AfterDOCTYPEPublicIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_after_doctype_public_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BetweenDOCTYPEPublicAndSystemIdentifiers;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '"') {
        parse_error("missing-whitespace-between-doctype-public-and-system-identifiers"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-between-doctype-public-and-system-identifiers"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_between_doctype_public_and_system_identifiers_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_after_doctype_system_keyword_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPESystemIdentifier;
    } else if (*cp == '"') {
        parse_error("missing-whitespace-after-doctype-system-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-after-doctype-system-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_before_doctype_system_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_doctype_system_identifier_double_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '"') {
        m_state = TokenizerState::AfterDOCTYPESystemIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_doctype_system_identifier_single_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\'') {
        m_state = TokenizerState::AfterDOCTYPESystemIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_after_doctype_system_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("unexpected-character-after-doctype-system-identifier"_s);
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_bogus_doctype_state() {
    auto cp = peek();
    if (!cp) {
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
    } else {
    }
}

} // namespace lithium::html
