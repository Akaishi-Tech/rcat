// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// OCaml + F# — (* ... *) comments, "..." strings.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW_OCAML[] = {
    "and","as","assert","asr","begin","class","constraint","do","done","downto",
    "else","end","exception","external","false","for","fun","function","functor",
    "if","in","include","inherit","initializer","land","lazy","let","lor","lsl",
    "lsr","lxor","match","method","mod","module","mutable","new","nonrec","object",
    "of","open","or","private","rec","sig","struct","then","to","true","try",
    "type","val","virtual","when","while","with",
    nullptr};
const char* const TY_OCAML[] = {
    "int","float","string","char","bool","unit","list","array","option","ref",
    "exn","bytes","int32","int64","nativeint",
    nullptr};
const char* const BI_OCAML[] = {
    "true","false","None","Some","Ok","Error","print_string","print_int",
    "print_endline","print_newline","raise","ignore","ref","List","Array",
    "String","Bytes","Printf","Format","Hashtbl","Map","Set",
    nullptr};

const char* const KW_FS[] = {
    "abstract","and","as","assert","base","begin","class","default","delegate",
    "do","done","downcast","downto","elif","else","end","exception","extern",
    "false","finally","fixed","for","fun","function","global","if","in",
    "inherit","inline","interface","internal","lazy","let","match","member",
    "module","mutable","namespace","new","null","of","open","or","override",
    "private","public","rec","return","sig","static","struct","then","to",
    "true","try","type","upcast","use","val","void","when","while","with","yield",
    nullptr};
const char* const TY_FS[] = {
    "int","int32","int64","float","float32","double","string","char","bool",
    "unit","list","array","option","seq","byte","sbyte","uint","uint32","uint64",
    "obj","exn","decimal","void",
    nullptr};
// reuse BI_OCAML mostly
const char* const BI_FS[] = {
    "true","false","None","Some","Ok","Error","printfn","printf","sprintf",
    "ignore","ref","List","Array","Seq","Map","Set",
    nullptr};

struct Spec {
    const char* const* kw;
    const char* const* ty;
    const char* const* bi;
};

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
        if (c == '(' && i + 1 < n && s[i + 1] == '*') {
            size_t j = i + 2;
            int depth = 1;
            while (j + 1 < n && depth > 0) {
                if (s[j] == '(' && s[j + 1] == '*') { depth++; j += 2; }
                else if (s[j] == '*' && s[j + 1] == ')') { depth--; j += 2; }
                else ++j;
            }
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            // F# line comment
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
        if (c == '\'') {
            // character literal 'x' or 'x'<type>
            if (i + 2 < n && s[i + 2] == '\'') {
                emit(TokenKind::String, s.substr(i, 3));
                i += 3;
                continue;
            }
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '~' && i + 1 < n && is_alpha(s[i + 1])) {
            // OCaml labeled argument
            size_t j = scan_ident(s, i + 1);
            emit(TokenKind::Attribute, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            // OCaml: identifiers may include ' at the end
            while (j < n && s[j] == '\'') ++j;
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, sp.kw))      kk = TokenKind::Keyword;
            else if (sp.ty && in_keyword_set(w, sp.ty)) kk = TokenKind::BuiltinType;
            else if (sp.bi && in_keyword_set(w, sp.bi)) kk = TokenKind::Builtin;
            else if (w.size() > 0 && w[0] >= 'A' && w[0] <= 'Z') kk = TokenKind::Constant;
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

const Spec OC = {KW_OCAML, TY_OCAML, BI_OCAML};
const Spec FS = {KW_FS,    TY_FS,    BI_FS};

} // namespace

void tokenise_ocaml (std::string_view s, const TokenSink& e) { scan(s, e, OC); }
void tokenise_fsharp(std::string_view s, const TokenSink& e) { scan(s, e, FS); }

} // namespace rcat::lang
