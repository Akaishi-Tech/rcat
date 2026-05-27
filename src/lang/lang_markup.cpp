// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// HTML / XML / SVG. They differ only in some preset tag names; the
// scanner itself is generic.

#include "lang_common.hpp"

namespace rcat::lang {

namespace {

void scan(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        // Comments <!-- ... -->
        if (starts_with(s, i, "<!--")) {
            size_t j = i + 4;
            while (j + 2 < n && !(s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>')) ++j;
            j = j + 2 < n ? j + 3 : n;
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        // CDATA
        if (starts_with(s, i, "<![CDATA[")) {
            size_t j = i + 9;
            while (j + 2 < n && !(s[j] == ']' && s[j + 1] == ']' && s[j + 2] == '>')) ++j;
            j = j + 2 < n ? j + 3 : n;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            continue;
        }
        // Doctype / processing instruction <! ... > or <? ... ?>
        if (c == '<' && i + 1 < n && (s[i + 1] == '!' || s[i + 1] == '?')) {
            size_t j = i + 1;
            while (j < n && s[j] != '>') ++j;
            if (j < n) ++j;
            emit(TokenKind::Preprocessor, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '<') {
            // tag open
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            bool closing = false;
            if (i < n && s[i] == '/') {
                emit(TokenKind::Punctuation, s.substr(i, 1));
                ++i;
                closing = true;
            }
            // tag name
            size_t name_start = i;
            while (i < n && (is_ident_cont(s[i]) || s[i] == '-' || s[i] == ':')) ++i;
            if (i > name_start) {
                emit(TokenKind::Tag, s.substr(name_start, i - name_start));
            }
            // attributes
            while (i < n && s[i] != '>' && s[i] != '\n') {
                if (is_space(s[i])) {
                    size_t j = scan_while(s, i, is_space);
                    emit(TokenKind::Text, s.substr(i, j - i));
                    i = j;
                    continue;
                }
                if (s[i] == '/') {
                    emit(TokenKind::Punctuation, s.substr(i, 1));
                    ++i;
                    continue;
                }
                // attribute name
                size_t aname = i;
                while (i < n && (is_ident_cont(s[i]) || s[i] == '-'
                                || s[i] == ':')) ++i;
                if (i > aname) {
                    emit(TokenKind::Attribute, s.substr(aname, i - aname));
                }
                if (i < n && s[i] == '=') {
                    emit(TokenKind::Operator, s.substr(i, 1));
                    ++i;
                    if (i < n && (s[i] == '"' || s[i] == '\'')) {
                        auto r = scan_simple_string(s, i, s[i], true);
                        emit_split_newlines(emit, TokenKind::String,
                                            s.substr(i, r.end - i));
                        i = r.end;
                    } else {
                        // unquoted value
                        size_t v = i;
                        while (i < n && !is_space(s[i]) && s[i] != '>'
                               && s[i] != '/' && s[i] != '\n') ++i;
                        if (i > v) emit(TokenKind::String, s.substr(v, i - v));
                    }
                }
                if (i < n && s[i] != '>' && !is_space(s[i]) && s[i] != '\n'
                    && s[i] != '=' && s[i] != '/') {
                    // unknown char — emit and bump
                    emit(TokenKind::Operator, s.substr(i, 1));
                    ++i;
                }
            }
            if (i < n && s[i] == '>') {
                emit(TokenKind::Punctuation, s.substr(i, 1));
                ++i;
            }
            (void)closing;
            continue;
        }
        // entity &amp; / &#123;
        if (c == '&') {
            size_t j = i + 1;
            while (j < n && s[j] != ';' && s[j] != '\n' && is_ident_cont(s[j]))
                ++j;
            if (j < n && s[j] == ';') {
                ++j;
                emit(TokenKind::StringEscape, s.substr(i, j - i));
                i = j;
                continue;
            }
        }
        // ordinary text up to next < or & or newline
        size_t j = i;
        while (j < n && s[j] != '<' && s[j] != '&' && s[j] != '\n') ++j;
        emit(TokenKind::Text, s.substr(i, j - i));
        i = j;
    }
}

} // namespace

void tokenise_html(std::string_view s, const TokenSink& e) { scan(s, e); }
void tokenise_xml (std::string_view s, const TokenSink& e) { scan(s, e); }
void tokenise_svg (std::string_view s, const TokenSink& e) { scan(s, e); }

} // namespace rcat::lang
