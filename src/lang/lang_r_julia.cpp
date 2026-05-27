// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// R + Julia tokeniser. Both use #-comments and similar number/identifier
// syntax; keyword sets differ enough to keep as two entry points sharing
// the bulk of the scanner.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

struct Spec {
    const char* const* keywords;
    const char* const* types;
    const char* const* builtins;
};

const char* const kw_r[] = {
    "if","else","for","while","repeat","function","return","break","next",
    "in","TRUE","FALSE","NULL","NA","NA_integer_","NA_real_","NA_character_",
    "Inf","NaN","library","require",
    nullptr};
const char* const ty_r[] = {
    "numeric","integer","double","character","logical","complex","raw","list",
    "vector","matrix","data.frame","factor","NULL",
    nullptr};
const char* const bi_r[] = {
    "TRUE","FALSE","NULL","NA","Inf","NaN","c","print","cat","paste","paste0",
    "sprintf","length","names","attr","class","is.numeric","is.character",
    "is.logical","sum","mean","median","sd","var","min","max","sort","unique",
    "lapply","sapply","mapply","apply","do.call",
    nullptr};

const char* const kw_jl[] = {
    "abstract","baremodule","begin","break","catch","ccall","const","continue",
    "do","else","elseif","end","export","false","finally","for","function",
    "global","if","import","importall","in","let","local","macro","module",
    "mutable","primitive","quote","return","struct","true","try","type","using",
    "while","where",
    nullptr};
const char* const ty_jl[] = {
    "Int","Int8","Int16","Int32","Int64","Int128","UInt","UInt8","UInt16",
    "UInt32","UInt64","UInt128","Float16","Float32","Float64","Bool","Char",
    "String","Symbol","Array","Vector","Matrix","Dict","Set","Tuple","Nothing",
    "Any","Number","Real","Integer","AbstractFloat","AbstractString",
    nullptr};
const char* const bi_jl[] = {
    "true","false","nothing","missing","println","print","push!","pop!","map",
    "filter","reduce","collect","length","size","typeof","isa","eltype",
    nullptr};

void scan(std::string_view s, const TokenSink& emit, const Spec& sp) {
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
            // Julia: #= ... =# block comments
            if (i + 1 < n && s[i + 1] == '=') {
                size_t j = i + 2;
                while (j + 1 < n && !(s[j] == '=' && s[j + 1] == '#')) ++j;
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
        if (c == '"' || c == '\'') {
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
            // Julia identifiers may end with ! or ?
            if (j < n && (s[j] == '!' || s[j] == '?')) ++j;
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, sp.keywords))      kk = TokenKind::Keyword;
            else if (in_keyword_set(word, sp.types))    kk = TokenKind::BuiltinType;
            else if (in_keyword_set(word, sp.builtins)) kk = TokenKind::Builtin;
            else {
                size_t k = j;
                while (k < n && (s[k] == ' ' || s[k] == '\t')) ++k;
                if (k < n && s[k] == '(') kk = TokenKind::Function;
            }
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

} // namespace

void tokenise_r    (std::string_view s, const TokenSink& e) { scan(s, e, {kw_r,  ty_r,  bi_r});  }
void tokenise_julia(std::string_view s, const TokenSink& e) { scan(s, e, {kw_jl, ty_jl, bi_jl}); }

} // namespace rcat::lang
