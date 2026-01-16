/**
 * Line Breaker implementation
 */

#include "lithium/text/line_breaker.hpp"
#include "lithium/text/font.hpp"

namespace lithium::text {

// ============================================================================
// Line Breaking
// ============================================================================

enum class BreakOpportunity {
    NoBreak,       // No break allowed
    BreakAllowed,  // Break allowed (soft break)
    BreakMandatory // Must break (newline)
};

// Unicode line breaking categories (simplified)
enum class LineBreakClass {
    AL,  // Alphabetic
    BA,  // Break After
    BB,  // Break Before
    BK,  // Mandatory Break
    CR,  // Carriage Return
    GL,  // Non-breaking ("Glue")
    HY,  // Hyphen
    LF,  // Line Feed
    NU,  // Numeric
    OP,  // Opening Punctuation
    CL,  // Close Punctuation
    SP,  // Space
    WJ,  // Word Joiner
    ZW,  // Zero Width Space
    Other
};

LineBreakClass get_line_break_class(unicode::CodePoint cp) {
    // Simplified classification
    if (cp == '\n') return LineBreakClass::LF;
    if (cp == '\r') return LineBreakClass::CR;
    if (cp == ' ') return LineBreakClass::SP;
    if (cp == '\t') return LineBreakClass::BA;
    if (cp == '-') return LineBreakClass::HY;
    if (cp >= '0' && cp <= '9') return LineBreakClass::NU;
    if (cp == '(' || cp == '[' || cp == '{') return LineBreakClass::OP;
    if (cp == ')' || cp == ']' || cp == '}') return LineBreakClass::CL;
    if (cp == 0x00A0) return LineBreakClass::GL;  // Non-breaking space
    if (cp == 0x200B) return LineBreakClass::ZW;  // Zero-width space
    if (cp == 0x2060) return LineBreakClass::WJ;  // Word joiner
    return LineBreakClass::AL;
}

BreakOpportunity get_break_opportunity(LineBreakClass before, LineBreakClass after) {
    // LB4: BK!
    if (before == LineBreakClass::BK) return BreakOpportunity::BreakMandatory;

    // LB5: CR x LF
    if (before == LineBreakClass::CR && after == LineBreakClass::LF) {
        return BreakOpportunity::NoBreak;
    }

    // LB5: CR!, LF!
    if (before == LineBreakClass::CR || before == LineBreakClass::LF) {
        return BreakOpportunity::BreakMandatory;
    }

    // LB7: x SP
    if (after == LineBreakClass::SP) return BreakOpportunity::NoBreak;

    // LB11: x WJ, WJ x
    if (before == LineBreakClass::WJ || after == LineBreakClass::WJ) {
        return BreakOpportunity::NoBreak;
    }

    // LB12: GL x
    if (before == LineBreakClass::GL) return BreakOpportunity::NoBreak;

    // LB13: x CL
    if (after == LineBreakClass::CL) return BreakOpportunity::NoBreak;

    // LB14: OP SP* x
    if (before == LineBreakClass::OP) return BreakOpportunity::NoBreak;

    // LB18: SP /
    if (before == LineBreakClass::SP) return BreakOpportunity::BreakAllowed;

    // LB21: x BA, HY x
    if (after == LineBreakClass::BA) return BreakOpportunity::NoBreak;
    if (before == LineBreakClass::HY) return BreakOpportunity::NoBreak;

    // LB28: AL x AL
    if (before == LineBreakClass::AL && after == LineBreakClass::AL) {
        return BreakOpportunity::NoBreak;
    }

    // LB23: NU x AL, AL x NU
    if ((before == LineBreakClass::NU && after == LineBreakClass::AL) ||
        (before == LineBreakClass::AL && after == LineBreakClass::NU)) {
        return BreakOpportunity::NoBreak;
    }

    // LB31: Default break allowed
    return BreakOpportunity::BreakAllowed;
}

// ============================================================================
// LineBreaker class
// ============================================================================

struct TextLine {
    String text;
    f32 width;
    usize start_index;
    usize end_index;
};

class LineBreaker {
public:
    LineBreaker() = default;

    std::vector<TextLine> break_lines(const String& text, f32 max_width, const Font& font) {
        std::vector<TextLine> lines;
        if (text.empty()) {
            return lines;
        }

        usize line_start = 0;
        usize last_break = 0;
        f32 line_width = 0;
        f32 width_at_break = 0;

        LineBreakClass prev_class = LineBreakClass::Other;

        for (usize i = 0; i < text.size(); ++i) {
            unicode::CodePoint cp = static_cast<unicode::CodePoint>(
                static_cast<unsigned char>(text[i]));
            LineBreakClass curr_class = get_line_break_class(cp);

            f32 char_width = font.measure_char(cp);
            BreakOpportunity opportunity = get_break_opportunity(prev_class, curr_class);

            // Check for mandatory break
            if (opportunity == BreakOpportunity::BreakMandatory) {
                TextLine line;
                line.start_index = line_start;
                line.end_index = i;
                line.text = text.substr(line_start, i - line_start);
                line.width = line_width;
                lines.push_back(line);

                line_start = i + 1;
                last_break = i + 1;
                line_width = 0;
                width_at_break = 0;
                prev_class = curr_class;
                continue;
            }

            // Record break opportunity
            if (opportunity == BreakOpportunity::BreakAllowed) {
                last_break = i;
                width_at_break = line_width;
            }

            // Check if we exceed max width
            if (line_width + char_width > max_width && last_break > line_start) {
                // Break at last opportunity
                TextLine line;
                line.start_index = line_start;
                line.end_index = last_break;
                line.text = text.substr(line_start, last_break - line_start);
                line.width = width_at_break;
                lines.push_back(line);

                // Skip whitespace after break
                line_start = last_break;
                while (line_start < text.size() && text[line_start] == ' ') {
                    ++line_start;
                }
                last_break = line_start;
                line_width = 0;
                width_at_break = 0;

                // Re-measure from new start
                for (usize j = line_start; j <= i; ++j) {
                    unicode::CodePoint jcp = static_cast<unicode::CodePoint>(
                        static_cast<unsigned char>(text[j]));
                    line_width += font.measure_char(jcp);
                }
            } else {
                line_width += char_width;
            }

            prev_class = curr_class;
        }

        // Add final line
        if (line_start < text.size()) {
            TextLine line;
            line.start_index = line_start;
            line.end_index = text.size();
            line.text = text.substr(line_start);
            line.width = line_width;
            lines.push_back(line);
        }

        return lines;
    }

    // Word wrap mode (break only at word boundaries)
    std::vector<TextLine> word_wrap(const String& text, f32 max_width, const Font& font) {
        return break_lines(text, max_width, font);
    }

    // Character wrap mode (break anywhere if needed)
    std::vector<TextLine> char_wrap(const String& text, f32 max_width, const Font& font) {
        std::vector<TextLine> lines;
        if (text.empty()) return lines;

        usize line_start = 0;
        f32 line_width = 0;

        for (usize i = 0; i < text.size(); ++i) {
            unicode::CodePoint cp = static_cast<unicode::CodePoint>(
                static_cast<unsigned char>(text[i]));

            if (cp == '\n') {
                TextLine line;
                line.start_index = line_start;
                line.end_index = i;
                line.text = text.substr(line_start, i - line_start);
                line.width = line_width;
                lines.push_back(line);

                line_start = i + 1;
                line_width = 0;
                continue;
            }

            f32 char_width = font.measure_char(cp);

            if (line_width + char_width > max_width && line_width > 0) {
                TextLine line;
                line.start_index = line_start;
                line.end_index = i;
                line.text = text.substr(line_start, i - line_start);
                line.width = line_width;
                lines.push_back(line);

                line_start = i;
                line_width = char_width;
            } else {
                line_width += char_width;
            }
        }

        if (line_start < text.size()) {
            TextLine line;
            line.start_index = line_start;
            line.end_index = text.size();
            line.text = text.substr(line_start);
            line.width = line_width;
            lines.push_back(line);
        }

        return lines;
    }
};

} // namespace lithium::text
