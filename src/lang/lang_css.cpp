// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// CSS / SCSS / Less. The lexical layer is the same; SCSS/Less add $vars /
// @vars and nesting, but those are just one extra token class each.

#include "lang_common.hpp"

namespace rcat::lang {

namespace {

void scan(std::string_view s, const TokenSink& emit, bool scss, bool less) {
    size_t i = 0, n = s.size();
    bool in_value = false;  // after ':' until ';' or '}'
    int brace = 0;
    while (i < n) {
        char c = s[i];
        if (c == '\n') {
            emit(TokenKind::Text, s.substr(i, 1));
            ++i;
            continue;
        }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '/'))
                ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if ((scss || less) && c == '/' && i + 1 < n && s[i + 1] == '/') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '{') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            ++brace;
            in_value = false;
            continue;
        }
        if (c == '}') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            --brace;
            in_value = false;
            continue;
        }
        if (c == ';') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            in_value = false;
            continue;
        }
        if (c == ':') {
            emit(TokenKind::Operator, s.substr(i, 1));
            ++i;
            if (brace > 0)
                in_value = true;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (c == '@') {
            size_t j = scan_while(s, i + 1, is_ident_cont);
            emit(TokenKind::Decorator, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (scss && c == '$') {
            size_t j = scan_while(s, i + 1, is_ident_cont);
            emit(TokenKind::Variable, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '#' && i + 6 < n) {
            // color literal #abc or #abcdef
            size_t j = i + 1;
            while (j < n && is_hex(s[j]))
                ++j;
            size_t hex = j - i - 1;
            if (hex == 3 || hex == 4 || hex == 6 || hex == 8) {
                emit(TokenKind::Number, s.substr(i, j - i));
                i = j;
                continue;
            }
        }
        if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t j = scan_number(s, i);
            // unit (px, em, %, deg, etc.)
            while (j < n && (is_alpha(s[j]) || s[j] == '%'))
                ++j;
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c) || c == '-' || c == '.' || c == '#') {
            size_t j = i;
            while (j < n && (is_ident_cont(s[j]) || s[j] == '-' || s[j] == '.' || s[j] == '#' ||
                             s[j] == ':'))
                ++j;
            std::string_view word = s.substr(i, j - i);
            TokenKind kk;
            if (in_value) {
                kk = TokenKind::Builtin;  // keyword values (auto, none, inherit)
            } else if (brace == 0) {
                kk = TokenKind::Selector;
            } else {
                kk = TokenKind::Property;
            }
            emit(kk, word);
            i = j;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
    (void)less;
}

}  // namespace

void tokenise_css(std::string_view s, const TokenSink& e) {
    scan(s, e, false, false);
}
void tokenise_scss(std::string_view s, const TokenSink& e) {
    scan(s, e, true, false);
}
void tokenise_less(std::string_view s, const TokenSink& e) {
    scan(s, e, true, true);
}

}  // namespace rcat::lang
