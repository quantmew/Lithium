#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <vector>

namespace lithium::text {

// ============================================================================
// Line Break Types (UAX #14)
// ============================================================================

enum class LineBreakClass {
    // Non-tailorable
    BK,  // Mandatory Break
    CR,  // Carriage Return
    LF,  // Line Feed
    CM,  // Combining Mark
    NL,  // Next Line
    SG,  // Surrogate
    WJ,  // Word Joiner
    ZW,  // Zero Width Space
    GL,  // Non-breaking (Glue)
    SP,  // Space

    // Break opportunities
    ZWJ, // Zero Width Joiner
    B2,  // Break Opportunity Before and After
    BA,  // Break After
    BB,  // Break Before
    HY,  // Hyphen
    CB,  // Contingent Break Opportunity

    // Characters prohibiting certain breaks
    CL,  // Close Punctuation
    CP,  // Close Parenthesis
    EX,  // Exclamation/Interrogation
    IN,  // Inseparable
    NS,  // Nonstarter
    OP,  // Open Punctuation
    QU,  // Quotation

    // Numeric
    IS,  // Infix Numeric Separator
    NU,  // Numeric
    PO,  // Postfix Numeric
    PR,  // Prefix Numeric
    SY,  // Symbols Allowing Break After

    // Alphabetic
    AI,  // Ambiguous (Alphabetic or Ideographic)
    AL,  // Alphabetic
    CJ,  // Conditional Japanese Starter
    EB,  // Emoji Base
    EM,  // Emoji Modifier
    H2,  // Hangul LV Syllable
    H3,  // Hangul LVT Syllable
    HL,  // Hebrew Letter
    ID,  // Ideographic
    JL,  // Hangul L Jamo
    JT,  // Hangul T Jamo
    JV,  // Hangul V Jamo
    RI,  // Regional Indicator
    SA,  // Complex Context Dependent
    XX,  // Unknown
};

// ============================================================================
// Break Opportunity
// ============================================================================

struct BreakOpportunity {
    usize offset;
    bool mandatory;
};

// ============================================================================
// Line Breaker
// ============================================================================

class LineBreaker {
public:
    LineBreaker();

    // Find all break opportunities in text
    [[nodiscard]] std::vector<BreakOpportunity> find_breaks(const String& text) const;

    // Check if break is allowed at position
    [[nodiscard]] bool can_break_at(const String& text, usize offset) const;

    // Get line break class for code point
    [[nodiscard]] static LineBreakClass get_line_break_class(unicode::CodePoint cp);

private:
    // UAX #14 pair table lookup
    [[nodiscard]] bool should_break(LineBreakClass before, LineBreakClass after) const;
};

// ============================================================================
// Word Breaker (UAX #29)
// ============================================================================

struct WordBoundary {
    usize offset;
    bool is_word_start;
};

class WordBreaker {
public:
    WordBreaker();

    // Find all word boundaries
    [[nodiscard]] std::vector<WordBoundary> find_boundaries(const String& text) const;

    // Extract words
    [[nodiscard]] std::vector<String> extract_words(const String& text) const;

private:
    enum class WordBreakProperty {
        Other,
        CR,
        LF,
        Newline,
        Extend,
        ZWJ,
        Regional_Indicator,
        Format,
        Katakana,
        Hebrew_Letter,
        ALetter,
        Single_Quote,
        Double_Quote,
        MidNumLet,
        MidLetter,
        MidNum,
        Numeric,
        ExtendNumLet,
        WSegSpace,
    };

    [[nodiscard]] static WordBreakProperty get_word_break_property(unicode::CodePoint cp);
};

// ============================================================================
// Grapheme Cluster Breaker (UAX #29)
// ============================================================================

class GraphemeBreaker {
public:
    // Find grapheme cluster boundaries
    [[nodiscard]] std::vector<usize> find_boundaries(const String& text) const;

    // Count grapheme clusters
    [[nodiscard]] usize count_graphemes(const String& text) const;

    // Get grapheme at index
    [[nodiscard]] String grapheme_at(const String& text, usize index) const;

private:
    enum class GraphemeBreakProperty {
        Other,
        CR,
        LF,
        Control,
        Extend,
        ZWJ,
        Regional_Indicator,
        Prepend,
        SpacingMark,
        L,
        V,
        T,
        LV,
        LVT,
    };

    [[nodiscard]] static GraphemeBreakProperty get_grapheme_break_property(unicode::CodePoint cp);
};

} // namespace lithium::text
