// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// JSON and JSON5 (JSON5 = JSON + comments + trailing commas + unquoted keys
// + single-quoted strings). Same scanner, JSON5 just enables extra rules.

#include "lang_common.hpp"

#include <vector>

namespace rcat::lang {

namespace {

void scan(std::string_view s, const TokenSink& emit, bool json5) {
    size_t i = 0, n = s.size();
    // Container stack: 'o' = object (next string is a key), 'a' = array.
    std::vector<char> stack;
    stack.push_back('o');  // top-level: assume object until first '['
    bool expect_key = true;
    bool after_colon = false;

    auto recompute_expect_key = [&]() {
        // In an object, expect a key unless we just consumed ':' and are
        // waiting for the value. In an array, never expect a key.
        if (stack.empty() || stack.back() != 'o') {
            expect_key = false;
        } else {
            expect_key = !after_colon;
        }
    };

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
        if (json5 && c == '/' && i + 1 < n && (s[i + 1] == '/' || s[i + 1] == '*')) {
            if (s[i + 1] == '/') {
                size_t j = scan_line(s, i);
                emit(TokenKind::Comment, s.substr(i, j - i));
                i = j;
            } else {
                size_t j = i + 2;
                while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '/'))
                    ++j;
                j = j + 1 < n ? j + 2 : n;
                emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
                i = j;
            }
            continue;
        }
        if (c == '{') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            stack.push_back('o');
            after_colon = false;
            recompute_expect_key();
            continue;
        }
        if (c == '[') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            stack.push_back('a');
            after_colon = false;
            recompute_expect_key();
            continue;
        }
        if (c == '}' || c == ']') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            if (!stack.empty())
                stack.pop_back();
            after_colon = false;
            recompute_expect_key();
            continue;
        }
        if (c == ',') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            after_colon = false;
            recompute_expect_key();
            continue;
        }
        if (c == ':') {
            emit(TokenKind::Operator, s.substr(i, 1));
            ++i;
            after_colon = true;
            recompute_expect_key();
            continue;
        }
        if (c == '"' || (json5 && c == '\'')) {
            auto r = scan_simple_string(s, i, c, false);
            TokenKind kk = expect_key ? TokenKind::Attribute : TokenKind::String;
            emit_split_newlines(emit, kk, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (c == '-' || c == '+' || is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t start = i;
            if (c == '-' || c == '+')
                ++i;
            if (i < n && (is_digit(s[i]) || s[i] == '.')) {
                size_t j = scan_number(s, i);
                emit(TokenKind::Number, s.substr(start, j - start));
                i = j;
                continue;
            }
            i = start;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view word = s.substr(i, j - i);
            TokenKind kk;
            if (word == "true" || word == "false" || word == "null" ||
                (json5 && (word == "Infinity" || word == "NaN" || word == "undefined"))) {
                kk = TokenKind::Builtin;
            } else if (json5 && expect_key) {
                kk = TokenKind::Attribute;
            } else {
                kk = TokenKind::Text;
            }
            emit(kk, word);
            i = j;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

}  // namespace

void tokenise_json(std::string_view s, const TokenSink& e) {
    scan(s, e, false);
}
void tokenise_json5(std::string_view s, const TokenSink& e) {
    scan(s, e, true);
}

}  // namespace rcat::lang
