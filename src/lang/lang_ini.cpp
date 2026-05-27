// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// INI-style configs: classic INI, .env, Java .properties. They share so
// much line-shape logic (comment, [section], key=value or key: value) that
// one scanner with knobs covers all three.

#include "lang_common.hpp"

namespace rcat::lang {

namespace {

struct Spec {
    const char* line_comment_prefixes;   // chars that introduce a comment
    bool        allow_colon_kv;          // accept "key: value" as well as "key=value"
    bool        sections;                // accept "[section]"
};

void scan(std::string_view s, const TokenSink& emit, const Spec& sp) {
    size_t i = 0, n = s.size();
    while (i < n) {
        // indent
        size_t bol = i;
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i > bol) emit(TokenKind::Text, s.substr(bol, i - bol));

        if (i >= n) break;
        char c = s[i];

        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }

        bool is_comment = false;
        for (const char* p = sp.line_comment_prefixes; p && *p; ++p) {
            if (c == *p) { is_comment = true; break; }
        }
        if (is_comment) {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }

        if (sp.sections && c == '[') {
            size_t j = i;
            while (j < n && s[j] != '\n' && s[j] != ']') ++j;
            if (j < n && s[j] == ']') ++j;
            emit(TokenKind::Section, s.substr(i, j - i));
            i = j;
            continue;
        }

        // key=value or key:value
        size_t k = i;
        while (k < n && s[k] != '\n' && s[k] != '=' && (!(sp.allow_colon_kv && s[k] == ':'))) {
            ++k;
        }
        if (k < n && (s[k] == '=' || (sp.allow_colon_kv && s[k] == ':'))) {
            // key
            size_t key_end = k;
            while (key_end > i && is_space(s[key_end - 1])) --key_end;
            if (key_end > i) emit(TokenKind::Attribute, s.substr(i, key_end - i));
            if (key_end < k) emit(TokenKind::Text, s.substr(key_end, k - key_end));
            // separator
            emit(TokenKind::Operator, s.substr(k, 1));
            i = k + 1;
            // optional space
            size_t sp_end = i;
            while (sp_end < n && (s[sp_end] == ' ' || s[sp_end] == '\t')) ++sp_end;
            if (sp_end > i) emit(TokenKind::Text, s.substr(i, sp_end - i));
            i = sp_end;
            // value to end of line
            size_t v = i;
            while (i < n && s[i] != '\n') {
                // Inline comment? Some dialects support # or ; after value.
                bool inline_comment = false;
                if (i > v && (s[i - 1] == ' ' || s[i - 1] == '\t')) {
                    for (const char* p = sp.line_comment_prefixes; p && *p; ++p) {
                        if (s[i] == *p) { inline_comment = true; break; }
                    }
                }
                if (inline_comment) break;
                ++i;
            }
            if (i > v) {
                std::string_view val = s.substr(v, i - v);
                std::string_view t = val;
                while (!t.empty() && is_space(t.back())) t.remove_suffix(1);
                TokenKind kk = TokenKind::String;
                if (t == "true" || t == "false" || t == "null" || t == "True"
                    || t == "False" || t == "yes" || t == "no") {
                    kk = TokenKind::Builtin;
                } else if (!t.empty() && (is_digit(t.front())
                                          || (t.front() == '-' && t.size() > 1 && is_digit(t[1])))) {
                    bool numeric = true;
                    for (char ch : t) {
                        if (!(is_digit(ch) || ch == '.' || ch == '-' || ch == '+')) {
                            numeric = false;
                            break;
                        }
                    }
                    if (numeric) kk = TokenKind::Number;
                }
                emit(kk, val);
            }
            // inline comment (if any)
            if (i < n && s[i] != '\n') {
                size_t j = scan_line(s, i);
                emit(TokenKind::Comment, s.substr(i, j - i));
                i = j;
            }
            continue;
        }
        // fallback: rest of line as text
        size_t j = i;
        while (j < n && s[j] != '\n') ++j;
        emit(TokenKind::Text, s.substr(i, j - i));
        i = j;
    }
}

const Spec INI = {";#", true,  true};
const Spec ENV = {"#",  false, false};
const Spec PROPS = {"#!", true, false};

} // namespace

void tokenise_ini       (std::string_view s, const TokenSink& e) { scan(s, e, INI);   }
void tokenise_env       (std::string_view s, const TokenSink& e) { scan(s, e, ENV);   }
void tokenise_properties(std::string_view s, const TokenSink& e) { scan(s, e, PROPS); }

} // namespace rcat::lang
