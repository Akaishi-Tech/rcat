// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// YAML — line-oriented format. We tokenise each line by looking at its
// shape (key: value, list item, anchor/alias, document marker). Full
// YAML is huge; this catches the 95% case visible in config files.

#include "lang_common.hpp"

namespace rcat::lang {

namespace {

bool peek_word_then_colon(std::string_view s, size_t i, size_t& key_end) {
    // returns true if [i .. key_end) is an identifier-ish run followed by ':'
    // (with optional spaces). YAML keys may contain spaces and hyphens, but
    // we keep it conservative.
    size_t j = i;
    while (j < s.size()) {
        char c = s[j];
        if (c == ':' || c == '\n' || c == '#') break;
        ++j;
    }
    if (j >= s.size() || s[j] != ':') return false;
    // Must be followed by space, newline, or EOF (YAML 1.2 rule)
    if (j + 1 < s.size() && !is_space(s[j + 1]) && s[j + 1] != '\n') return false;
    // strip trailing whitespace from the key span
    while (j > i && is_space(s[j - 1])) --j;
    key_end = j;
    return j > i;
}

} // namespace

void tokenise_yaml(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        // emit any leading indent as plain text
        size_t bol = i;
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i > bol) emit(TokenKind::Text, s.substr(bol, i - bol));

        if (i >= n) break;
        char c = s[i];

        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        // document markers
        if ((starts_with(s, i, "---") || starts_with(s, i, "...")) &&
            (i + 3 == n || s[i + 3] == '\n' || s[i + 3] == ' ')) {
            emit(TokenKind::Preprocessor, s.substr(i, 3));
            i += 3;
            continue;
        }
        // list item: "- "
        if (c == '-' && i + 1 < n && (s[i + 1] == ' ' || s[i + 1] == '\n')) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }

        // key: value
        size_t key_end = i;
        if (peek_word_then_colon(s, i, key_end)) {
            emit(TokenKind::Attribute, s.substr(i, key_end - i));
            i = key_end;
            // colon and possible spaces are emitted by the line-rest scanner
            emit(TokenKind::Operator, s.substr(i, 1));
            ++i;
            // Whitespace after colon
            size_t j = i;
            while (j < n && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (j > i) emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            // Value: emit rest of the line as value. Use simple heuristic:
            // quoted strings stay String; bare scalars become Text; numbers
            // become Number; reserved literals become Builtin.
            if (i < n && (s[i] == '"' || s[i] == '\'')) {
                auto r = scan_simple_string(s, i, s[i], false);
                emit(TokenKind::String, s.substr(i, r.end - i));
                i = r.end;
                continue;
            }
            // Bare scalar to end of line
            size_t v = i;
            while (i < n && s[i] != '\n' && s[i] != '#') ++i;
            std::string_view val = s.substr(v, i - v);
            // Trim trailing spaces from val for classification
            std::string_view t = val;
            while (!t.empty() && is_space(t.back())) t.remove_suffix(1);
            if (t == "true" || t == "false" || t == "null" || t == "~"
                || t == "True" || t == "False" || t == "Null"
                || t == "yes"  || t == "no"    || t == "on"   || t == "off") {
                emit(TokenKind::Builtin, val);
            } else if (!t.empty() && (is_digit(t.front()) || t.front() == '-')) {
                bool numeric = true;
                for (char ch : t) {
                    if (!(is_digit(ch) || ch == '.' || ch == '-' || ch == '+'
                          || ch == 'e' || ch == 'E')) { numeric = false; break; }
                }
                emit(numeric ? TokenKind::Number : TokenKind::Text, val);
            } else {
                emit(TokenKind::Text, val);
            }
            continue;
        }

        // anchors / aliases / tags
        if (c == '&' || c == '*' || c == '!') {
            size_t j = i + 1;
            while (j < n && (is_ident_cont(s[j]) || s[j] == '!')) ++j;
            emit(TokenKind::Decorator, s.substr(i, j - i));
            i = j;
            continue;
        }

        // fallback: rest of line as text
        size_t j = i;
        while (j < n && s[j] != '\n') ++j;
        emit(TokenKind::Text, s.substr(i, j - i));
        i = j;
    }
}

} // namespace rcat::lang
