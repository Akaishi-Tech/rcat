// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

void emit_split_newlines(const TokenSink& emit, TokenKind kind,
                         std::string_view text) {
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            if (i > start) emit(kind, text.substr(start, i - start));
            emit(TokenKind::Text, std::string_view("\n", 1));
            start = i + 1;
        }
    }
    if (start < text.size()) emit(kind, text.substr(start));
}

bool in_keyword_set(std::string_view word, const char* const* set) {
    for (const char* const* p = set; *p; ++p) {
        std::string_view kw(*p);
        if (kw.size() == word.size()
            && std::memcmp(kw.data(), word.data(), word.size()) == 0) {
            return true;
        }
    }
    return false;
}

size_t scan_line(std::string_view s, size_t i) {
    while (i < s.size() && s[i] != '\n') ++i;
    return i;
}

size_t scan_ident(std::string_view s, size_t i) {
    return scan_while(s, i, is_ident_cont);
}

size_t scan_number(std::string_view s, size_t i) {
    size_t n = s.size();
    if (i >= n) return i;

    // 0x / 0b / 0o prefix
    if (s[i] == '0' && i + 1 < n) {
        char p = s[i + 1];
        if (p == 'x' || p == 'X') {
            i += 2;
            while (i < n && (is_hex(s[i]) || s[i] == '_')) ++i;
            // hex float exponent (0x1.8p3)
            if (i < n && s[i] == '.') {
                ++i;
                while (i < n && (is_hex(s[i]) || s[i] == '_')) ++i;
            }
            if (i < n && (s[i] == 'p' || s[i] == 'P')) {
                ++i;
                if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
                while (i < n && (is_digit(s[i]) || s[i] == '_')) ++i;
            }
            // suffix (u, l, ul, ull, L, f, ...)
            while (i < n && (is_alpha(s[i]) || s[i] == '_')) ++i;
            return i;
        }
        if (p == 'b' || p == 'B' || p == 'o' || p == 'O') {
            i += 2;
            while (i < n && (is_digit(s[i]) || s[i] == '_')) ++i;
            while (i < n && (is_alpha(s[i]) || s[i] == '_')) ++i;
            return i;
        }
    }

    // Integer part
    while (i < n && (is_digit(s[i]) || s[i] == '_')) ++i;

    // Fractional part
    if (i < n && s[i] == '.' && i + 1 < n && is_digit(s[i + 1])) {
        ++i;
        while (i < n && (is_digit(s[i]) || s[i] == '_')) ++i;
    }

    // Exponent
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
        while (i < n && (is_digit(s[i]) || s[i] == '_')) ++i;
    }

    // Numeric suffix (u, l, ll, f, d, n, i, ...) and unit (e.g. 1.5px is one
    // token in many style languages — the per-language tokeniser can override
    // this if it cares about a tighter parse).
    while (i < n && (is_alpha(s[i]) || s[i] == '_')) ++i;
    return i;
}

StringScan scan_simple_string(std::string_view s, size_t i,
                              char quote, bool allow_newline) {
    StringScan r{i, false};
    size_t n = s.size();
    if (i >= n || s[i] != quote) return r;
    ++i;
    while (i < n) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) {
            // Skip an escape — newline-only continuations are language-specific
            // (e.g. bash), but a generic "\<char>" handles the vast majority.
            i += 2;
            continue;
        }
        if (c == quote) {
            r.end = i + 1;
            r.terminated = true;
            return r;
        }
        if (c == '\n' && !allow_newline) {
            r.end = i;       // do NOT consume the newline
            return r;
        }
        ++i;
    }
    r.end = i;
    return r;
}

} // namespace rcat::lang
