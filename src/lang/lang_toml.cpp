// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// TOML tokeniser. Sections in [brackets], key = value, basic and
// multiline strings, dates/numbers.

#include "lang_common.hpp"

namespace rcat::lang {

void tokenise_toml(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    bool at_bol = true;
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; at_bol = true; continue; }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (at_bol && c == '[') {
            size_t j = i;
            while (j < n && s[j] != '\n' && s[j] != ']') ++j;
            if (j < n && s[j] == ']') ++j;
            // include possible [[array]] second bracket
            if (j < n && s[j] == ']') ++j;
            emit(TokenKind::Section, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            // triple-quoted multiline?
            if (i + 2 < n && s[i + 1] == c && s[i + 2] == c) {
                size_t j = i + 3;
                while (j + 2 < n
                       && !(s[j] == c && s[j + 1] == c && s[j + 2] == c)) ++j;
                j = j + 2 < n ? j + 3 : n;
                emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
                i = j;
                at_bol = false;
                continue;
            }
            auto r = scan_simple_string(s, i, c, false);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (at_bol && (is_ident_start(c) || c == '-')) {
            // key = value
            size_t j = i;
            while (j < n && (is_ident_cont(s[j]) || s[j] == '-' || s[j] == '.')) ++j;
            emit(TokenKind::Attribute, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '=') {
            emit(TokenKind::Operator, s.substr(i, 1));
            ++i;
            at_bol = false;
            continue;
        }
        if (is_digit(c) || (c == '-' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t j = scan_number(s, i + (c == '-' ? 1 : 0));
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (w == "true" || w == "false" || w == "inf" || w == "nan")
                kk = TokenKind::Builtin;
            emit(kk, w);
            i = j;
            continue;
        }
        emit(TokenKind::Punctuation, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

} // namespace rcat::lang
