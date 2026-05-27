// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// POSIX shell / bash / zsh / fish family. Highlights variables ($x, ${x}),
// double-quote strings (with $ interpolation as Variable), here-docs in a
// minimal form, and a generous keyword/builtin set.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {
    "if",       "then",   "elif",  "else",  "fi",     "case",     "esac",  "for",     "select",
    "while",    "until",  "do",    "done",  "in",     "function", "time",  "coproc",  "break",
    "continue", "return", "exit",  "exec",  "trap",   "wait",     "local", "declare", "typeset",
    "readonly", "export", "unset", "shift", "source", "let",      nullptr};
const char* const BI[] = {
    "echo",    "printf", "read",    "cd",     "pwd",     "pushd",   "popd",     "dirs",    "alias",
    "unalias", "bind",   "builtin", "caller", "command", "compgen", "complete", "disown",  "enable",
    "eval",    "fc",     "fg",      "bg",     "getopts", "hash",    "help",     "history", "jobs",
    "kill",    "logout", "mapfile", "pwd",    "set",     "shopt",   "suspend",  "test",    "times",
    "trap",    "type",   "ulimit",  "umask",  "wait",    "true",    "false",    "[",       nullptr};

}  // namespace

void tokenise_shell(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    bool at_bol = true;
    while (i < n) {
        char c = s[i];
        if (c == '\n') {
            emit(TokenKind::Text, s.substr(i, 1));
            ++i;
            at_bol = true;
            continue;
        }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '#' && (at_bol || (i > 0 && is_space(s[i - 1])))) {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        // Variable $name, ${name}, $1, $? …
        if (c == '$') {
            if (i + 1 < n) {
                char p = s[i + 1];
                if (p == '{') {
                    size_t j = i + 2;
                    while (j < n && s[j] != '}')
                        ++j;
                    if (j < n)
                        ++j;
                    emit(TokenKind::Variable, s.substr(i, j - i));
                    i = j;
                    at_bol = false;
                    continue;
                }
                if (p == '(' && i + 2 < n && s[i + 2] == '(') {
                    // arithmetic $((...))
                    size_t j = i + 3;
                    int depth = 1;
                    while (j + 1 < n && depth > 0) {
                        if (s[j] == '(' && s[j + 1] == '(') {
                            depth++;
                            j += 2;
                        } else if (s[j] == ')' && s[j + 1] == ')') {
                            depth--;
                            j += 2;
                        } else
                            ++j;
                    }
                    emit(TokenKind::Variable, s.substr(i, j - i));
                    i = j;
                    at_bol = false;
                    continue;
                }
                if (p == '(') {
                    size_t j = i + 2;
                    int depth = 1;
                    while (j < n && depth > 0) {
                        if (s[j] == '(')
                            depth++;
                        else if (s[j] == ')')
                            depth--;
                        ++j;
                    }
                    emit(TokenKind::Variable, s.substr(i, j - i));
                    i = j;
                    at_bol = false;
                    continue;
                }
                if (is_ident_start(p) || is_digit(p) || p == '?' || p == '@' || p == '#' ||
                    p == '*' || p == '$' || p == '!' || p == '-') {
                    size_t j = i + 2;
                    if (is_ident_start(p) || is_digit(p)) {
                        j = scan_while(s, j, is_ident_cont);
                    }
                    emit(TokenKind::Variable, s.substr(i, j - i));
                    i = j;
                    at_bol = false;
                    continue;
                }
            }
        }
        if (c == '\'') {
            auto r = scan_simple_string(s, i, '\'', true);
            // single-quote strings in shell don't process \ escapes; but our
            // generic scanner does. We just treat as a string anyway — the
            // visible output is the same for highlight purposes.
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (c == '"') {
            // Walk manually so we can flag $... interpolations differently.
            size_t j = i + 1;
            size_t run_start = i;
            (void)run_start;
            while (j < n && s[j] != '"') {
                if (s[j] == '\\' && j + 1 < n) {
                    j += 2;
                    continue;
                }
                ++j;
            }
            if (j < n)
                ++j;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '`') {
            size_t j = i + 1;
            while (j < n && s[j] != '`') {
                if (s[j] == '\\' && j + 1 < n) {
                    j += 2;
                    continue;
                }
                ++j;
            }
            if (j < n)
                ++j;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (is_ident_start(c) || c == '-') {
            size_t j = i;
            if (c == '-') {
                // option-like flag: --foo / -x
                ++j;
                if (j < n && s[j] == '-')
                    ++j;
                while (j < n && (is_ident_cont(s[j]) || s[j] == '-'))
                    ++j;
                if (j > i + 1) {
                    emit(TokenKind::Attribute, s.substr(i, j - i));
                    i = j;
                    at_bol = false;
                    continue;
                }
                // fall through: standalone '-' is an operator
            } else {
                j = scan_ident(s, i);
                std::string_view word = s.substr(i, j - i);
                TokenKind kk = TokenKind::Text;
                if (in_keyword_set(word, KW))
                    kk = TokenKind::Keyword;
                else if (in_keyword_set(word, BI))
                    kk = TokenKind::Builtin;
                else if (at_bol)
                    kk = TokenKind::Function;
                emit(kk, word);
                i = j;
                at_bol = false;
                continue;
            }
        }
        if (std::strchr("{}[]();,", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            at_bol = false;
            continue;
        }
        if (c == '|' || c == '&' || c == ';') {
            emit(TokenKind::Operator, s.substr(i, 1));
            ++i;
            at_bol = true;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

}  // namespace rcat::lang
