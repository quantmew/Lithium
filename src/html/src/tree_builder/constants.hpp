#pragma once

#include "lithium/core/string.hpp"
#include "lithium/core/types.hpp"
#include <array>

namespace lithium::html::detail {

inline constexpr std::array<const char*, 82> SPECIAL_ELEMENTS = {
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

inline constexpr std::array<const char*, 14> FORMATTING_ELEMENTS = {
    "a", "b", "big", "code", "em", "font", "i", "nobr", "s", "small",
    "strike", "strong", "tt", "u"
};

inline constexpr std::array<const char*, 10> IMPLIED_END_TAG_ELEMENTS = {
    "dd", "dt", "li", "optgroup", "option", "p", "rb", "rp", "rt", "rtc"
};

inline bool is_ascii_whitespace(unicode::CodePoint cp) {
    return cp == '\t' || cp == '\n' || cp == '\f' || cp == '\r' || cp == ' ';
}

} // namespace lithium::html::detail
