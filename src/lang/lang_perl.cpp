// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Perl tokeniser. Recognises sigils ($scalar, @array, %hash, &sub) and
// here-docs in their most common form.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {
    "and",      "cmp",      "continue",    "do",   "else",    "elsif",  "eq",      "exp",
    "for",      "foreach",  "ge",          "gt",   "if",      "last",   "le",      "local",
    "lt",       "my",       "ne",          "next", "no",      "not",    "or",      "our",
    "package",  "print",    "printf",      "redo", "require", "return", "say",     "sub",
    "unless",   "until",    "use",         "when", "while",   "xor",    "__END__", "__DATA__",
    "__FILE__", "__LINE__", "__PACKAGE__", nullptr};
const char* const TY[] = {nullptr};
const char* const BI[] = {
    "abs",   "bless",   "chomp",  "chop",      "chr",  "close",  "defined", "die",
    "eval",  "exists",  "grep",   "hex",       "int",  "join",   "keys",    "length",
    "map",   "open",    "ord",    "pop",       "push", "ref",    "reverse", "scalar",
    "shift", "sort",    "split",  "sprintf",   "sqrt", "substr", "uc",      "ucfirst",
    "undef", "unshift", "values", "wantarray", "warn", nullptr};

}  // namespace

void tokenise_perl(std::string_view s, const TokenSink& emit) {
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
        // POD: =pod ... =cut
        if (c == '=' && (i == 0 || s[i - 1] == '\n') && i + 1 < n && is_alpha(s[i + 1])) {
            size_t j = i;
            while (j < n) {
                if (s[j] == '\n' && j + 1 < n && starts_with(s, j + 1, "=cut")) {
                    j = j + 1;
                    while (j < n && s[j] != '\n')
                        ++j;
                    break;
                }
                ++j;
            }
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        // sigils
        if (c == '$' || c == '@' || c == '%' || c == '&') {
            if (i + 1 < n && (is_ident_start(s[i + 1]) || s[i + 1] == '{')) {
                size_t j = i + 1;
                if (s[j] == '{') {
                    ++j;
                    while (j < n && s[j] != '}')
                        ++j;
                    if (j < n)
                        ++j;
                } else {
                    j = scan_ident(s, j);
                }
                emit(TokenKind::Variable, s.substr(i, j - i));
                i = j;
                continue;
            }
        }
        if (c == '"' || c == '\'' || c == '`') {
            auto r = scan_simple_string(s, i, c, true);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
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
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, KW))
                kk = TokenKind::Keyword;
            else if (in_keyword_set(word, TY))
                kk = TokenKind::BuiltinType;
            else if (in_keyword_set(word, BI))
                kk = TokenKind::Builtin;
            emit(kk, word);
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
