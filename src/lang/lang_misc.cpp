// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Diff / patch / git-config / .gitignore / vimscript / markdown-as-source /
// LaTeX. Each is small; grouping keeps the project navigable while still
// honouring the "one entry per language" contract.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

void scan_diff(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        size_t bol = i;
        size_t eol = i;
        while (eol < n && s[eol] != '\n') ++eol;
        std::string_view line = s.substr(bol, eol - bol);
        TokenKind kk = TokenKind::Text;
        if (line.size() >= 3 && (line.compare(0, 3, "+++") == 0
                              || line.compare(0, 3, "---") == 0)) {
            kk = TokenKind::Heading;
        } else if (line.size() >= 2 && line.compare(0, 2, "@@") == 0) {
            kk = TokenKind::Section;
        } else if (!line.empty() && line.front() == '+') {
            kk = TokenKind::String;
        } else if (!line.empty() && line.front() == '-') {
            kk = TokenKind::Error;
        } else if (line.size() >= 4 && line.compare(0, 4, "diff") == 0) {
            kk = TokenKind::Keyword;
        } else if (line.size() >= 5 && line.compare(0, 5, "index") == 0) {
            kk = TokenKind::Comment;
        }
        if (!line.empty()) emit(kk, line);
        if (eol < n) {
            emit(TokenKind::Text, s.substr(eol, 1));
            ++eol;
        }
        i = eol;
    }
}

} // namespace

void tokenise_diff (std::string_view s, const TokenSink& e) { scan_diff(s, e); }
void tokenise_patch(std::string_view s, const TokenSink& e) { scan_diff(s, e); }

void tokenise_gitconfig(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        size_t bol = i;
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i > bol) emit(TokenKind::Text, s.substr(bol, i - bol));
        if (i >= n) break;
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (c == '#' || c == ';') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '[') {
            size_t j = i;
            while (j < n && s[j] != ']' && s[j] != '\n') ++j;
            if (j < n) ++j;
            emit(TokenKind::Section, s.substr(i, j - i));
            i = j;
            continue;
        }
        // key = value
        size_t j = i;
        while (j < n && s[j] != '=' && s[j] != '\n') ++j;
        if (j < n && s[j] == '=') {
            size_t key_end = j;
            while (key_end > i && is_space(s[key_end - 1])) --key_end;
            emit(TokenKind::Attribute, s.substr(i, key_end - i));
            if (key_end < j) emit(TokenKind::Text, s.substr(key_end, j - key_end));
            emit(TokenKind::Operator, s.substr(j, 1));
            i = j + 1;
            size_t v = i;
            while (i < n && s[i] != '\n') ++i;
            if (i > v) emit(TokenKind::String, s.substr(v, i - v));
        } else {
            i = j;
        }
    }
}

void tokenise_gitignore(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '!') {
            emit(TokenKind::Decorator, s.substr(i, 1));
            ++i;
            continue;
        }
        size_t j = i;
        while (j < n && s[j] != '\n') ++j;
        emit(TokenKind::String, s.substr(i, j - i));
        i = j;
    }
}

void tokenise_vimscript(std::string_view s, const TokenSink& emit) {
    static const char* const KW[] = {
        "if","elseif","else","endif","while","endwhile","for","endfor",
        "function","endfunction","return","let","unlet","set","setlocal",
        "setglobal","map","nmap","vmap","imap","cmap","noremap","nnoremap",
        "vnoremap","inoremap","cnoremap","autocmd","augroup","command","echo",
        "echom","execute","call","try","catch","endtry","finally","throw",
        "source","runtime","syntax","colorscheme","highlight",
        nullptr};
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (c == '"') {
            // Vim uses " for line comments (and " also for strings — Vim
            // disambiguates by context). For an unanchored highlighter we
            // treat " at the start of an expression context as a comment.
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '\'') {
            auto r = scan_simple_string(s, i, '\'', false);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c) || c == ':') {
            size_t j = i;
            if (c == ':') ++j;
            j = scan_while(s, j, is_ident_cont);
            std::string_view w = s.substr(i, j - i);
            std::string_view stripped = w;
            if (!stripped.empty() && stripped.front() == ':') stripped.remove_prefix(1);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(stripped, KW)) kk = TokenKind::Keyword;
            emit(kk, w);
            i = j;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

void tokenise_markdown(std::string_view s, const TokenSink& emit) {
    // Render markdown source highlighted (when used INSIDE another markdown
    // file's code fence). Highlight headings, list markers, links, code
    // fences, and emphasis markers.
    size_t i = 0, n = s.size();
    bool at_bol = true;
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; at_bol = true; continue; }
        if (at_bol && c == '#') {
            // ATX heading
            size_t j = i;
            while (j < n && s[j] == '#') ++j;
            size_t end = scan_line(s, j);
            emit(TokenKind::Heading, s.substr(i, end - i));
            i = end;
            at_bol = false;
            continue;
        }
        if (at_bol && (c == '*' || c == '-' || c == '+')
            && i + 1 < n && s[i + 1] == ' ') {
            emit(TokenKind::Punctuation, s.substr(i, 2));
            i += 2;
            at_bol = false;
            continue;
        }
        if (at_bol && c == '>') {
            emit(TokenKind::Decorator, s.substr(i, 1));
            ++i;
            continue;
        }
        if (c == '`') {
            // code span
            size_t j = i + 1;
            while (j < n && s[j] != '`' && s[j] != '\n') ++j;
            if (j < n && s[j] == '`') ++j;
            emit(TokenKind::String, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '[') {
            size_t j = i;
            while (j < n && s[j] != ']' && s[j] != '\n') ++j;
            if (j < n) ++j;
            if (j < n && s[j] == '(') {
                while (j < n && s[j] != ')' && s[j] != '\n') ++j;
                if (j < n) ++j;
            }
            emit(TokenKind::Url, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '*' || c == '_') {
            emit(TokenKind::Emphasis, s.substr(i, 1));
            ++i;
            at_bol = false;
            continue;
        }
        size_t j = i;
        while (j < n && s[j] != '\n' && s[j] != '`' && s[j] != '[' && s[j] != '*' && s[j] != '_') {
            ++j;
        }
        emit(TokenKind::Text, s.substr(i, j - i));
        i = j;
        at_bol = false;
    }
}

void tokenise_latex(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (c == '%') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '\\') {
            // command \name or \,
            size_t j = i + 1;
            if (j < n && is_alpha(s[j])) {
                while (j < n && is_alpha(s[j])) ++j;
            } else if (j < n) {
                ++j;
            }
            emit(TokenKind::Keyword, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$' && i + 1 < n && s[i + 1] == '$') {
            // display math $$
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == '$' && s[j + 1] == '$')) ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$') {
            size_t j = i + 1;
            while (j < n && s[j] != '$' && s[j] != '\n') ++j;
            if (j < n) ++j;
            emit(TokenKind::String, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '{' || c == '}') {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        size_t j = i;
        while (j < n && s[j] != '\\' && s[j] != '%' && s[j] != '$'
               && s[j] != '{' && s[j] != '}' && s[j] != '\n') ++j;
        emit(TokenKind::Text, s.substr(i, j - i));
        i = j;
    }
}

} // namespace rcat::lang
