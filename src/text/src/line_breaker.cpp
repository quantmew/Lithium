/**
 * Line/Word/Grapheme breaker implementations (simplified to keep the build green)
 */

#include "lithium/text/line_breaker.hpp"
#include "lithium/core/string.hpp"
#include <algorithm>

namespace lithium::text {

namespace {

bool is_mandatory_break(LineBreakClass cls) {
    return cls == LineBreakClass::BK ||
           cls == LineBreakClass::CR ||
           cls == LineBreakClass::LF ||
           cls == LineBreakClass::NL;
}

} // namespace

LineBreaker::LineBreaker() = default;

LineBreakClass LineBreaker::get_line_break_class(unicode::CodePoint cp) {
    if (cp == '\n') return LineBreakClass::LF;
    if (cp == '\r') return LineBreakClass::CR;
    if (cp == 0x0B) return LineBreakClass::BK;
    if (cp == ' ') return LineBreakClass::SP;
    if (cp == '\t') return LineBreakClass::BA;
    if (cp == '-') return LineBreakClass::HY;
    if (cp == 0x00A0) return LineBreakClass::GL;
    if (unicode::is_ascii_digit(cp)) return LineBreakClass::NU;
    if (cp == '(' || cp == '[' || cp == '{') return LineBreakClass::OP;
    if (cp == ')' || cp == ']' || cp == '}') return LineBreakClass::CL;
    return LineBreakClass::AL;
}

bool LineBreaker::should_break(LineBreakClass before, LineBreakClass after) const {
    if (is_mandatory_break(before)) {
        return true;
    }

    if (before == LineBreakClass::SP ||
        before == LineBreakClass::BA ||
        before == LineBreakClass::BB ||
        before == LineBreakClass::HY) {
        return true;
    }

    if (after == LineBreakClass::GL ||
        after == LineBreakClass::WJ ||
        after == LineBreakClass::ZWJ) {
        return false;
    }

    return false;
}

std::vector<BreakOpportunity> LineBreaker::find_breaks(const String& text) const {
    std::vector<BreakOpportunity> breaks;
    if (text.empty()) {
        return breaks;
    }

    const char* ptr = text.data();
    const char* end = ptr + text.size();

    auto first = unicode::utf8_decode(ptr, static_cast<usize>(end - ptr));
    LineBreakClass prev_class = get_line_break_class(first.code_point);
    usize offset = first.bytes_consumed;
    ptr += first.bytes_consumed;

    while (ptr < end) {
        auto decoded = unicode::utf8_decode(ptr, static_cast<usize>(end - ptr));
        if (decoded.bytes_consumed == 0) {
            break;
        }

        LineBreakClass curr_class = get_line_break_class(decoded.code_point);
        if (is_mandatory_break(prev_class)) {
            breaks.push_back({offset, true});
        } else if (should_break(prev_class, curr_class)) {
            breaks.push_back({offset, false});
        }

        prev_class = curr_class;
        ptr += decoded.bytes_consumed;
        offset += decoded.bytes_consumed;
    }

    breaks.push_back({text.size(), true});
    return breaks;
}

bool LineBreaker::can_break_at(const String& text, usize offset) const {
    auto breaks = find_breaks(text);
    return std::any_of(breaks.begin(), breaks.end(), [offset](const BreakOpportunity& br) {
        return br.offset == offset;
    });
}

WordBreaker::WordBreaker() = default;

WordBreaker::WordBreakProperty WordBreaker::get_word_break_property(unicode::CodePoint cp) {
    if (unicode::is_ascii_whitespace(cp)) return WordBreakProperty::WSegSpace;
    if (unicode::is_ascii_digit(cp)) return WordBreakProperty::Numeric;
    if (unicode::is_ascii_alpha(cp)) return WordBreakProperty::ALetter;
    return WordBreakProperty::Other;
}

std::vector<WordBoundary> WordBreaker::find_boundaries(const String& text) const {
    std::vector<WordBoundary> boundaries;
    const char* ptr = text.data();
    const char* end = ptr + text.size();

    bool in_word = false;
    usize offset = 0;

    auto is_word_character = [](WordBreakProperty prop) {
        switch (prop) {
            case WordBreakProperty::ALetter:
            case WordBreakProperty::Hebrew_Letter:
            case WordBreakProperty::Numeric:
            case WordBreakProperty::Katakana:
                return true;
            default:
                return false;
        }
    };

    while (ptr < end) {
        auto decoded = unicode::utf8_decode(ptr, static_cast<usize>(end - ptr));
        if (decoded.bytes_consumed == 0) {
            break;
        }

        auto prop = get_word_break_property(decoded.code_point);
        bool word_char = is_word_character(prop);

        if (word_char && !in_word) {
            boundaries.push_back({offset, true});
            in_word = true;
        } else if (!word_char && in_word) {
            boundaries.push_back({offset, false});
            in_word = false;
        }

        ptr += decoded.bytes_consumed;
        offset += decoded.bytes_consumed;
    }

    if (in_word) {
        boundaries.push_back({text.size(), false});
    }

    return boundaries;
}

std::vector<String> WordBreaker::extract_words(const String& text) const {
    std::vector<String> words;
    const char* ptr = text.data();
    const char* end = ptr + text.size();

    bool in_word = false;
    usize start = 0;
    usize offset = 0;

    auto is_word_character = [](WordBreakProperty prop) {
        switch (prop) {
            case WordBreakProperty::ALetter:
            case WordBreakProperty::Hebrew_Letter:
            case WordBreakProperty::Numeric:
            case WordBreakProperty::Katakana:
                return true;
            default:
                return false;
        }
    };

    while (ptr < end) {
        auto decoded = unicode::utf8_decode(ptr, static_cast<usize>(end - ptr));
        if (decoded.bytes_consumed == 0) {
            break;
        }

        auto prop = get_word_break_property(decoded.code_point);
        bool word_char = is_word_character(prop);

        if (word_char && !in_word) {
            in_word = true;
            start = offset;
        } else if (!word_char && in_word) {
            words.push_back(text.substring(start, offset - start));
            in_word = false;
        }

        ptr += decoded.bytes_consumed;
        offset += decoded.bytes_consumed;
    }

    if (in_word) {
        words.push_back(text.substring(start));
    }

    return words;
}

std::vector<usize> GraphemeBreaker::find_boundaries(const String& text) const {
    std::vector<usize> boundaries;
    const char* ptr = text.data();
    const char* end = ptr + text.size();

    usize offset = 0;
    while (ptr < end) {
        boundaries.push_back(offset);

        auto decoded = unicode::utf8_decode(ptr, static_cast<usize>(end - ptr));
        if (decoded.bytes_consumed == 0) {
            break;
        }

        ptr += decoded.bytes_consumed;
        offset += decoded.bytes_consumed;
    }

    boundaries.push_back(text.size());
    return boundaries;
}

usize GraphemeBreaker::count_graphemes(const String& text) const {
    auto boundaries = find_boundaries(text);
    if (boundaries.size() < 2) {
        return 0;
    }
    return boundaries.size() - 1;
}

String GraphemeBreaker::grapheme_at(const String& text, usize index) const {
    auto boundaries = find_boundaries(text);
    if (boundaries.size() < 2 || index + 1 >= boundaries.size()) {
        return ""_s;
    }
    return text.substring(boundaries[index], boundaries[index + 1] - boundaries[index]);
}

GraphemeBreaker::GraphemeBreakProperty GraphemeBreaker::get_grapheme_break_property(unicode::CodePoint cp) {
    if (unicode::is_ascii_whitespace(cp)) {
        return GraphemeBreakProperty::Control;
    }
    return GraphemeBreakProperty::Other;
}

} // namespace lithium::text
