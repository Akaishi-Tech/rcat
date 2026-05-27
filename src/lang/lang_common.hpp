// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include "../highlight.hpp"

#include <functional>
#include <string_view>

namespace rcat::lang {

// A tokeniser receives the full source and emits one call per token.
// Tokens never cross newlines: a span ending mid-line is emitted up to the
// '\n', then the '\n' is emitted on its own (as Text), then the next-line
// portion is emitted. This keeps highlight.cpp's per-line prefixing simple.
using TokenSink = std::function<void(TokenKind, std::string_view)>;

using TokeniseFn = void (*)(std::string_view source, const TokenSink& emit);

// Look up the tokeniser registered for `lang`. Returns nullptr if none.
TokeniseFn get_tokeniser(Language lang);

// Convenience: emit `text`, splitting at every '\n' so that no emitted token
// straddles a newline. Used by simple tokenisers for runs that may contain
// embedded newlines (block comments, multi-line strings).
void emit_split_newlines(const TokenSink& emit, TokenKind kind,
                         std::string_view text);

// ---------------------------------------------------------------------------
// Character classification — ASCII only on purpose. Non-ASCII bytes inside
// identifiers/strings just pass through as part of the surrounding run.

inline bool is_digit(char c)        { return c >= '0' && c <= '9'; }
inline bool is_hex(char c)          { return is_digit(c)
                                          || (c >= 'a' && c <= 'f')
                                          || (c >= 'A' && c <= 'F'); }
inline bool is_alpha(char c)        { return (c >= 'a' && c <= 'z')
                                          || (c >= 'A' && c <= 'Z'); }
inline bool is_ident_start(char c)  { return is_alpha(c) || c == '_'
                                          || (unsigned char)c >= 0x80; }
inline bool is_ident_cont(char c)   { return is_ident_start(c) || is_digit(c); }
inline bool is_space(char c)        { return c == ' '  || c == '\t'
                                          || c == '\r' || c == '\f'
                                          || c == '\v'; }

// Match a contiguous run of `pred(c)` starting at `i`. Returns the end index.
template <typename Pred>
inline size_t scan_while(std::string_view s, size_t i, Pred pred) {
    while (i < s.size() && pred(s[i])) ++i;
    return i;
}

// Does `s` starting at `i` equal `lit`?
inline bool starts_with(std::string_view s, size_t i, std::string_view lit) {
    return s.size() - i >= lit.size()
        && s.compare(i, lit.size(), lit) == 0;
}

// True if `word` (ASCII) is in the sorted, NUL-terminated keyword set `set`
// (a contiguous list of NUL-separated keywords ended by a double NUL). The
// caller guarantees the set is sorted lexicographically. Linear scan is fine
// for keyword counts in the low hundreds and avoids hash-set allocations.
bool in_keyword_set(std::string_view word, const char* const* set);

// ---------------------------------------------------------------------------
// Building-block scanners. Each returns the index AFTER the lexeme. They
// emit nothing — the caller decides what TokenKind the result is.

// C-style line comment starting with `marker` (e.g. "//", "#", "--").
// Stops at end of line (does not consume the '\n').
size_t scan_line(std::string_view s, size_t i);

// Number: optional sign, integer/float, 0x/0b/0o prefix, exponent, suffix.
// `i` must point at a digit or at '.'/'-' followed by a digit.
size_t scan_number(std::string_view s, size_t i);

// Identifier: [is_ident_start][is_ident_cont]*
size_t scan_ident(std::string_view s, size_t i);

// String run for languages that use `quote` as a delimiter with `\` escapes.
// Does NOT cross newlines (returns end-of-line if string is unterminated).
// Set `allow_newline=true` for languages whose strings may span lines.
struct StringScan {
    size_t end;            // index past the closing quote (or EOL/EOF)
    bool   terminated;     // saw closing quote
};
StringScan scan_simple_string(std::string_view s, size_t i,
                              char quote, bool allow_newline);

} // namespace rcat::lang
