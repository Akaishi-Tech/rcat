// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Erlang + Elixir. Erlang: % comments, function/0 syntax. Elixir:
// # comments, :atoms, do…end blocks, @attribute, string sigils ~s, ~r.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW_ERL[] = {
    "after","and","andalso","band","begin","bnot","bor","bsl","bsr","bxor",
    "case","catch","cond","div","end","fun","if","let","not","of","or","orelse",
    "receive","rem","try","when","xor","-module","-export","-import","-define",
    "-record","-include","-spec","-type",
    nullptr};
const char* const BI_ERL[] = {
    "true","false","ok","error","undefined","nil","is_atom","is_binary",
    "is_boolean","is_float","is_function","is_integer","is_list","is_map",
    "is_number","is_pid","is_port","is_reference","is_tuple","length","tuple_size",
    "byte_size","atom_to_list","list_to_atom","integer_to_list","io","lists",
    "maps","binary","string","spawn","self","node","monitor","link","exit",
    nullptr};

const char* const KW_EX[] = {
    "after","and","case","catch","cond","def","defcallback","defdelegate",
    "defexception","defguard","defguardp","defimpl","defmacro","defmacrop",
    "defmodule","defp","defprotocol","defstruct","do","else","end","false",
    "fn","for","if","import","in","nil","not","or","quote","raise","receive",
    "require","rescue","return","throw","true","try","unless","unquote",
    "unquote_splicing","use","when","with","yield",
    nullptr};
const char* const BI_EX[] = {
    "true","false","nil","self","is_atom","is_binary","is_boolean","is_float",
    "is_function","is_integer","is_list","is_map","is_number","is_pid","is_tuple",
    "length","tuple_size","byte_size","to_string","to_charlist","IO","Enum",
    "String","Map","List","Tuple","Atom","Process","Task","GenServer","Supervisor",
    nullptr};

} // namespace

void tokenise_erlang(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
        if (is_space(c)) {
            size_t j = scan_while(s, i, is_space);
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '%') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
            TokenKind kk = c == '\'' ? TokenKind::Constant : TokenKind::String;
            emit(kk, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c) || c == '-') {
            size_t start = i;
            if (c == '-') ++i;
            i = scan_ident(s, i);
            std::string_view w = s.substr(start, i - start);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, KW_ERL))      kk = TokenKind::Keyword;
            else if (in_keyword_set(w, BI_ERL)) kk = TokenKind::Builtin;
            else if (w.size() && (w[0] >= 'A' && w[0] <= 'Z')) kk = TokenKind::Variable;
            emit(kk, w);
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

void tokenise_elixir(std::string_view s, const TokenSink& emit) {
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '\n') { emit(TokenKind::Text, s.substr(i, 1)); ++i; continue; }
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
            emit(TokenKind::Decorator, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == ':' && i + 1 < n && (is_ident_start(s[i + 1]) || s[i + 1] == '"')) {
            // :atom or :"atom"
            if (s[i + 1] == '"') {
                auto r = scan_simple_string(s, i + 1, '"', false);
                emit(TokenKind::Constant, s.substr(i, r.end - i));
                i = r.end;
            } else {
                size_t j = scan_ident(s, i + 1);
                while (j < n && (s[j] == '?' || s[j] == '!')) ++j;
                emit(TokenKind::Constant, s.substr(i, j - i));
                i = j;
            }
            continue;
        }
        if (c == '"') {
            if (i + 2 < n && s[i + 1] == '"' && s[i + 2] == '"') {
                size_t j = i + 3;
                while (j + 2 < n
                       && !(s[j] == '"' && s[j + 1] == '"' && s[j + 2] == '"')) ++j;
                j = j + 2 < n ? j + 3 : n;
                emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
                i = j;
                continue;
            }
            auto r = scan_simple_string(s, i, '"', false);
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
            while (j < n && (s[j] == '?' || s[j] == '!')) ++j;
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, KW_EX))      kk = TokenKind::Keyword;
            else if (in_keyword_set(w, BI_EX)) kk = TokenKind::Builtin;
            else if (w.size() && (w[0] >= 'A' && w[0] <= 'Z')) kk = TokenKind::Constant;
            else {
                size_t k = j;
                while (k < n && (s[k] == ' ' || s[k] == '\t')) ++k;
                if (k < n && s[k] == '(') kk = TokenKind::Function;
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

} // namespace rcat::lang
