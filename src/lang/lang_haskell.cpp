// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Haskell, Elm, Ada. Haskell/Elm share -- line, {- ... -} block comment.
// Ada uses -- line comments. Keep them as three entries.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW_HS[] = {"case",   "class",   "data",    "default",   "deriving", "do",
                             "else",   "forall",  "foreign", "hiding",    "if",       "import",
                             "in",     "infix",   "infixl",  "infixr",    "instance", "let",
                             "module", "newtype", "of",      "qualified", "then",     "type",
                             "where",  "_",       "family",  nullptr};
const char* const TY_HS[] = {"Bool",       "Char",  "Double", "Float", "Int",      "Integer",
                             "String",     "Maybe", "Either", "IO",    "Ordering", "Word",
                             "ByteString", "Text",  "Array",  nullptr};
const char* const BI_HS[] = {"True",   "False",   "Nothing", "Just",      "Left",   "Right", "LT",
                             "EQ",     "GT",      "return",  "putStrLn",  "putStr", "print", "read",
                             "show",   "map",     "filter",  "foldr",     "foldl",  "head",  "tail",
                             "length", "reverse", "null",    "undefined", "error",  nullptr};

const char* const KW_ELM[] = {"as",   "case",  "else",   "exposing", "if",   "import",
                              "in",   "let",   "module", "of",       "port", "then",
                              "type", "where", "alias",  nullptr};
const char* const TY_ELM[] = {"Bool",  "Char",    "Float",   "Int",  "String", "List",
                              "Maybe", "Result",  "Cmd",     "Sub",  "Html",   "Attribute",
                              "Json",  "Decoder", "Encoder", nullptr};
const char* const BI_ELM[] = {"True", "False", "Nothing", "Just",     "Ok",   "Err",
                              "LT",   "EQ",    "GT",      "identity", nullptr};

const char* const KW_ADA[] = {
    "abort",   "abs",        "abstract",  "accept",    "access",  "aliased",      "all",
    "and",     "array",      "at",        "begin",     "body",    "case",         "constant",
    "declare", "delay",      "delta",     "digits",    "do",      "else",         "elsif",
    "end",     "entry",      "exception", "exit",      "for",     "function",     "generic",
    "goto",    "if",         "in",        "interface", "is",      "limited",      "loop",
    "mod",     "new",        "not",       "null",      "of",      "or",           "others",
    "out",     "overriding", "package",   "pragma",    "private", "procedure",    "protected",
    "raise",   "range",      "record",    "rem",       "renames", "requeue",      "return",
    "reverse", "select",     "separate",  "some",      "subtype", "synchronized", "tagged",
    "task",    "terminate",  "then",      "type",      "until",   "use",          "when",
    "while",   "with",       "xor",       "true",      "false",   nullptr};

struct Spec {
    const char* const* kw;
    const char* const* ty;
    const char* const* bi;
    bool brace_block_comment;     // {- ... -}
    bool dash_dash_line_comment;  // --
};

void scan(std::string_view s, const TokenSink& emit, const Spec& sp) {
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
        if (sp.dash_dash_line_comment && c == '-' && i + 1 < n && s[i + 1] == '-') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (sp.brace_block_comment && c == '{' && i + 1 < n && s[i + 1] == '-') {
            size_t j = i + 2;
            int depth = 1;
            while (j + 1 < n && depth > 0) {
                if (s[j] == '{' && s[j + 1] == '-') {
                    depth++;
                    j += 2;
                } else if (s[j] == '-' && s[j + 1] == '}') {
                    depth--;
                    j += 2;
                } else
                    ++j;
            }
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"') {
            auto r = scan_simple_string(s, i, '"', false);
            emit(TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (c == '\'') {
            // Haskell: 'c' character literal; or part of an ident
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
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            // Haskell allows trailing ' on idents
            while (j < n && s[j] == '\'')
                ++j;
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, sp.kw))
                kk = TokenKind::Keyword;
            else if (sp.ty && in_keyword_set(w, sp.ty))
                kk = TokenKind::BuiltinType;
            else if (sp.bi && in_keyword_set(w, sp.bi))
                kk = TokenKind::Builtin;
            else if (w.size() > 0 && w[0] >= 'A' && w[0] <= 'Z')
                kk = TokenKind::Constant;
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

const Spec HS = {KW_HS, TY_HS, BI_HS, true, true};
const Spec ELM = {KW_ELM, TY_ELM, BI_ELM, true, true};
const Spec ADA = {KW_ADA, nullptr, nullptr, false, true};

}  // namespace

void tokenise_haskell(std::string_view s, const TokenSink& e) {
    scan(s, e, HS);
}
void tokenise_elm(std::string_view s, const TokenSink& e) {
    scan(s, e, ELM);
}
void tokenise_ada(std::string_view s, const TokenSink& e) {
    scan(s, e, ADA);
}

}  // namespace rcat::lang
