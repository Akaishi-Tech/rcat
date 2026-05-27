// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Makefile + Dockerfile + CMake + Nginx + Apache — one file per language,
// each is small enough to live alongside the others here. Splitting further
// is trivial: copy a tokenise_X function into its own .cpp and add the
// extern declaration in highlight.cpp.

#include "lang_common.hpp"

#include <cctype>
#include <cstring>

namespace rcat::lang {

namespace {

// shared bits ------------------------------------------------------------

bool eq_icase(std::string_view a, const char* b) {
    size_t i = 0;
    while (i < a.size() && b[i]) {
        char x = (char)std::tolower((unsigned char)a[i]);
        char y = (char)std::tolower((unsigned char)b[i]);
        if (x != y)
            return false;
        ++i;
    }
    return i == a.size() && b[i] == 0;
}

}  // namespace

void tokenise_makefile(std::string_view s, const TokenSink& emit) {
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
        if (at_bol && c == '\t') {
            // Recipe line — emit the whole line as text, but highlight $vars.
            size_t j = i;
            while (j < n && s[j] != '\n') {
                if (s[j] == '$' && j + 1 < n) {
                    if (j > i)
                        emit(TokenKind::Text, s.substr(i, j - i));
                    size_t k = j + 1;
                    if (s[k] == '(' || s[k] == '{') {
                        char close = s[k] == '(' ? ')' : '}';
                        ++k;
                        while (k < n && s[k] != close && s[k] != '\n')
                            ++k;
                        if (k < n)
                            ++k;
                    } else {
                        ++k;
                    }
                    emit(TokenKind::Variable, s.substr(j, k - j));
                    i = k;
                    j = k;
                    continue;
                }
                ++j;
            }
            if (j > i)
                emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$') {
            size_t k = i + 1;
            if (k < n && (s[k] == '(' || s[k] == '{')) {
                char close = s[k] == '(' ? ')' : '}';
                ++k;
                while (k < n && s[k] != close && s[k] != '\n')
                    ++k;
                if (k < n)
                    ++k;
            } else if (k < n) {
                ++k;
            }
            emit(TokenKind::Variable, s.substr(i, k - i));
            i = k;
            at_bol = false;
            continue;
        }
        if (at_bol && (is_ident_start(c) || c == '.')) {
            // target line: NAME [NAME...] : prereqs
            size_t j = i;
            while (j < n && s[j] != ':' && s[j] != '=' && s[j] != '\n' && s[j] != '#')
                ++j;
            if (j < n && s[j] == ':' && (j + 1 >= n || s[j + 1] != '=')) {
                emit(TokenKind::Function, s.substr(i, j - i));
                emit(TokenKind::Operator, s.substr(j, 1));
                i = j + 1;
                at_bol = false;
                continue;
            }
            if (j < n && (s[j] == '=' || (j + 1 < n && s[j] == ':' && s[j + 1] == '='))) {
                emit(TokenKind::Attribute, s.substr(i, j - i));
                size_t opl = s[j] == ':' ? 2 : 1;
                emit(TokenKind::Operator, s.substr(j, opl));
                i = j + opl;
                at_bol = false;
                continue;
            }
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (w == "include" || w == "ifeq" || w == "ifneq" || w == "ifdef" || w == "ifndef" ||
                w == "else" || w == "endif" || w == "define" || w == "endef" || w == "export") {
                kk = TokenKind::Keyword;
            }
            emit(kk, w);
            i = j;
            at_bol = false;
            continue;
        }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

void tokenise_dockerfile(std::string_view s, const TokenSink& emit) {
    static const char* const cmds[] = {
        "FROM",    "RUN",        "CMD",         "LABEL",  "MAINTAINER", "EXPOSE",  "ENV",
        "ADD",     "COPY",       "ENTRYPOINT",  "VOLUME", "USER",       "WORKDIR", "ARG",
        "ONBUILD", "STOPSIGNAL", "HEALTHCHECK", "SHELL",  nullptr};
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
        if (c == '#' && at_bol) {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (at_bol && is_alpha(c)) {
            size_t j = scan_ident(s, i);
            std::string_view w = s.substr(i, j - i);
            bool match = false;
            for (auto p = cmds; *p; ++p) {
                if (eq_icase(w, *p)) {
                    match = true;
                    break;
                }
            }
            emit(match ? TokenKind::Keyword : TokenKind::Text, w);
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '$') {
            size_t k = i + 1;
            if (k < n && s[k] == '{') {
                ++k;
                while (k < n && s[k] != '}' && s[k] != '\n')
                    ++k;
                if (k < n)
                    ++k;
            } else {
                k = scan_while(s, k, is_ident_cont);
            }
            emit(TokenKind::Variable, s.substr(i, k - i));
            i = k;
            at_bol = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, true);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        emit(TokenKind::Text, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

void tokenise_cmake(std::string_view s, const TokenSink& emit) {
    static const char* const cmds[] = {"add_executable",
                                       "add_library",
                                       "add_subdirectory",
                                       "add_definitions",
                                       "add_compile_options",
                                       "add_custom_command",
                                       "add_custom_target",
                                       "add_test",
                                       "include",
                                       "include_directories",
                                       "cmake_minimum_required",
                                       "project",
                                       "find_package",
                                       "find_path",
                                       "find_library",
                                       "target_link_libraries",
                                       "target_include_directories",
                                       "target_compile_options",
                                       "target_sources",
                                       "target_compile_definitions",
                                       "set",
                                       "unset",
                                       "option",
                                       "message",
                                       "if",
                                       "elseif",
                                       "else",
                                       "endif",
                                       "foreach",
                                       "endforeach",
                                       "while",
                                       "endwhile",
                                       "function",
                                       "endfunction",
                                       "macro",
                                       "endmacro",
                                       "return",
                                       "install",
                                       "configure_file",
                                       "pkg_check_modules",
                                       "execute_process",
                                       "file",
                                       "string",
                                       "list",
                                       "get_filename_component",
                                       "enable_testing",
                                       "set_tests_properties",
                                       nullptr};
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
        if (c == '"') {
            auto r = scan_simple_string(s, i, '"', true);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (c == '$' && i + 1 < n && s[i + 1] == '{') {
            size_t j = i + 2;
            while (j < n && s[j] != '}' && s[j] != '\n')
                ++j;
            if (j < n)
                ++j;
            emit(TokenKind::Variable, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = i;
            while (j < n && (is_ident_cont(s[j]) || s[j] == '-'))
                ++j;
            std::string_view w = s.substr(i, j - i);
            bool kw = false;
            for (auto p = cmds; *p; ++p) {
                if (eq_icase(w, *p)) {
                    kw = true;
                    break;
                }
            }
            TokenKind kk;
            if (kw)
                kk = TokenKind::Function;
            else {
                // Heuristic for upper-case constants (CMAKE_FOO)
                bool upper = true;
                for (char ch : w) {
                    if (!((ch >= 'A' && ch <= 'Z') || ch == '_' || is_digit(ch))) {
                        upper = false;
                        break;
                    }
                }
                kk = (upper && w.size() > 1) ? TokenKind::Constant : TokenKind::Text;
            }
            emit(kk, w);
            i = j;
            continue;
        }
        if (std::strchr("()", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

void tokenise_nginx(std::string_view s, const TokenSink& emit) {
    static const char* const dirs[] = {"server",
                                       "listen",
                                       "location",
                                       "root",
                                       "index",
                                       "server_name",
                                       "include",
                                       "upstream",
                                       "proxy_pass",
                                       "proxy_set_header",
                                       "return",
                                       "rewrite",
                                       "if",
                                       "set",
                                       "map",
                                       "ssl_certificate",
                                       "ssl_certificate_key",
                                       "access_log",
                                       "error_log",
                                       "worker_processes",
                                       "events",
                                       "http",
                                       "types",
                                       "default_type",
                                       "client_max_body_size",
                                       "add_header",
                                       "gzip",
                                       "try_files",
                                       "fastcgi_pass",
                                       "fastcgi_param",
                                       nullptr};
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
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$' && i + 1 < n && (is_ident_start(s[i + 1]) || s[i + 1] == '{')) {
            size_t j = i + 1;
            if (s[j] == '{') {
                ++j;
                while (j < n && s[j] != '}' && s[j] != '\n')
                    ++j;
                if (j < n)
                    ++j;
            } else {
                j = scan_while(s, j, is_ident_cont);
            }
            emit(TokenKind::Variable, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (at_bol && is_alpha(c)) {
            size_t j = scan_while(s, i, [](char ch) { return is_ident_cont(ch) || ch == '_'; });
            std::string_view w = s.substr(i, j - i);
            bool match = false;
            for (auto p = dirs; *p; ++p) {
                if (w == *p) {
                    match = true;
                    break;
                }
            }
            emit(match ? TokenKind::Keyword : TokenKind::Text, w);
            i = j;
            at_bol = false;
            continue;
        }
        if (std::strchr("{};", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            if (c == ';' || c == '{' || c == '}')
                at_bol = true;
            continue;
        }
        emit(TokenKind::Text, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

void tokenise_apache(std::string_view s, const TokenSink& emit) {
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
        if (c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '<') {
            // <Directory ...> or </Directory>
            size_t j = i;
            while (j < n && s[j] != '>' && s[j] != '\n')
                ++j;
            if (j < n)
                ++j;
            emit(TokenKind::Tag, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (at_bol && is_alpha(c)) {
            size_t j = scan_ident(s, i);
            emit(TokenKind::Keyword, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        emit(TokenKind::Text, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

}  // namespace rcat::lang
