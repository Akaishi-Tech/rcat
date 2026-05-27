// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Lua — -- and --[[ ... ]] comments; [[ ... ]] long strings.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {"and",      "break",  "do",   "else", "elseif", "end",   "false", "for",
                          "function", "goto",   "if",   "in",   "local",  "nil",   "not",   "or",
                          "repeat",   "return", "then", "true", "until",  "while", nullptr};
const char* const BI[] = {
    "print",  "require",  "pairs",     "ipairs",         "next",         "select",
    "type",   "tostring", "tonumber",  "setmetatable",   "getmetatable", "rawequal",
    "rawget", "rawset",   "rawlen",    "collectgarbage", "error",        "assert",
    "pcall",  "xpcall",   "unpack",    "table",          "string",       "math",
    "io",     "os",       "coroutine", "package",        "_G",           "_ENV",
    nullptr};

}  // namespace

void tokenise_lua(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
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
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {
            // Long comment --[[ ... ]] or line --
            if (i + 3 < n && s[i + 2] == '[' && s[i + 3] == '[') {
                size_t j = i + 4;
                while (j + 1 < n && !(s[j] == ']' && s[j + 1] == ']'))
                    ++j;
                j = j + 1 < n ? j + 2 : n;
                emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
                i = j;
                continue;
            }
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '[' && i + 1 < n && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == ']' && s[j + 1] == ']'))
                ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, KW))
                kk = TokenKind::Keyword;
            else if (in_keyword_set(w, BI))
                kk = TokenKind::Builtin;
            else {
                size_t k = j;
                while (k < n && (s[k] == ' ' || s[k] == '\t'))
                    ++k;
                if (k < n && s[k] == '(')
                    kk = TokenKind::Function;
            }
            emit(kk, w);
            i = j;
            continue;
        }
        if (std::strchr("{}[]();,", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

}  // namespace rcat::lang
