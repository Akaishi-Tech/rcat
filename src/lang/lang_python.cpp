// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Python tokeniser. Handles triple-quoted strings, f/r/b prefixes,
// decorators, and the `print` builtin among many others.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {
    "False",  "None",     "True",   "and", "as",    "assert", "async",  "await",    "break", "case",
    "class",  "continue", "def",    "del", "elif",  "else",   "except", "finally",  "for",   "from",
    "global", "if",       "import", "in",  "is",    "lambda", "match",  "nonlocal", "not",   "or",
    "pass",   "raise",    "return", "try", "while", "with",   "yield",  nullptr};
const char* const TY[] = {"bool",  "bytearray", "bytes", "complex", "dict",
                          "float", "frozenset", "int",   "list",    "object",
                          "set",   "str",       "tuple", "type",    nullptr};
const char* const BI[] = {
    "abs",          "all",  "any",         "ascii",   "bin",     "bool",       "breakpoint",
    "callable",     "chr",  "classmethod", "compile", "delattr", "dir",        "divmod",
    "enumerate",    "eval", "exec",        "filter",  "format",  "getattr",    "globals",
    "hasattr",      "hash", "help",        "hex",     "id",      "input",      "isinstance",
    "issubclass",   "iter", "len",         "locals",  "map",     "max",        "min",
    "next",         "oct",  "open",        "ord",     "pow",     "print",      "property",
    "range",        "repr", "reversed",    "round",   "setattr", "slice",      "sorted",
    "staticmethod", "sum",  "super",       "vars",    "zip",     "__import__", "self",
    "cls",          nullptr};

bool is_str_prefix_char(char c) {
    switch (c) {
    case 'r':
    case 'R':
    case 'b':
    case 'B':
    case 'u':
    case 'U':
    case 'f':
    case 'F':
        return true;
    default:
        return false;
    }
}

}  // namespace

void tokenise_python(std::string_view s, const TokenSink& emit) {
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
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '@' && i + 1 < n && is_ident_start(s[i + 1])) {
            size_t j = scan_ident(s, i + 1);
            // Allow dots in @module.deco
            while (j < n && s[j] == '.' && j + 1 < n && is_ident_start(s[j + 1])) {
                j = scan_ident(s, j + 1);
            }
            emit(TokenKind::Decorator, s.substr(i, j - i));
            continue;
        }
        // String prefix: r, b, f, u, rb, br, fr, rf, etc.
        if (is_str_prefix_char(c)) {
            size_t p = i;
            int got = 0;
            while (p < n && is_str_prefix_char(s[p]) && got < 2) {
                ++p;
                ++got;
            }
            if (p < n && (s[p] == '"' || s[p] == '\'')) {
                char q = s[p];
                size_t start = i;
                // Triple-quoted?
                if (p + 2 < n && s[p + 1] == q && s[p + 2] == q) {
                    size_t j = p + 3;
                    while (j + 2 < n && !(s[j] == q && s[j + 1] == q && s[j + 2] == q)) {
                        if (s[j] == '\\' && j + 1 < n)
                            j += 2;
                        else
                            ++j;
                    }
                    j = j + 2 < n ? j + 3 : n;
                    emit_split_newlines(emit, TokenKind::String, s.substr(start, j - start));
                    i = j;
                    continue;
                }
                auto r = scan_simple_string(s, p, q, false);
                emit_split_newlines(emit, TokenKind::String, s.substr(start, r.end - start));
                i = r.end;
                continue;
            }
        }
        if (c == '"' || c == '\'') {
            if (i + 2 < n && s[i + 1] == c && s[i + 2] == c) {
                size_t j = i + 3;
                while (j + 2 < n && !(s[j] == c && s[j + 1] == c && s[j + 2] == c)) {
                    if (s[j] == '\\' && j + 1 < n)
                        j += 2;
                    else
                        ++j;
                }
                j = j + 2 < n ? j + 3 : n;
                emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
                i = j;
                continue;
            }
            auto r = scan_simple_string(s, i, c, false);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, KW))
                kk = TokenKind::Keyword;
            else if (in_keyword_set(word, TY))
                kk = TokenKind::BuiltinType;
            else if (in_keyword_set(word, BI))
                kk = TokenKind::Builtin;
            else {
                size_t k = j;
                while (k < n && (s[k] == ' ' || s[k] == '\t'))
                    ++k;
                if (k < n && s[k] == '(')
                    kk = TokenKind::Function;
            }
            emit(kk, word);
            i = j;
            continue;
        }
        if (std::strchr("{}[]():,;", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

}  // namespace rcat::lang
