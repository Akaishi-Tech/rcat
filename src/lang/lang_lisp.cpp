// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Lisp, Scheme, Clojure, and assembly. All use ; for line comments
// (Lisp/Scheme/Clojure) or # ; varies for asm; we treat both '#' and ';'
// as line-comment introducers for asm to cover NASM/GAS dialects.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW_LISP[] = {"defun",        "defmacro",     "defvar",
                               "defparameter", "defconstant",  "defclass",
                               "defmethod",    "let",          "let*",
                               "let-values",   "let-syntax",   "lambda",
                               "function",     "quote",        "unquote",
                               "quasiquote",   "setq",         "setf",
                               "if",           "when",         "unless",
                               "cond",         "case",         "progn",
                               "prog1",        "and",          "or",
                               "not",          "do",           "dolist",
                               "dotimes",      "loop",         "while",
                               "return",       "return-from",  "block",
                               "handler-case", "handler-bind", "handler-bind*",
                               "catch",        "throw",        "unwind-protect",
                               nullptr};
const char* const BI_LISP[] = {"t",      "nil",    "car",    "cdr",         "cons",   "list",
                               "first",  "rest",   "null",   "atom",        "eq",     "eql",
                               "equal",  "equalp", "apply",  "funcall",     "reduce", "map",
                               "mapcar", "format", "print",  "princ",       "prin1",  "write",
                               "read",   "load",   "intern", "make-symbol", "gensym", nullptr};

const char* const KW_SCM[] = {"define",      "define-syntax", "define-record-type",
                              "lambda",      "let",           "let*",
                              "letrec",      "letrec*",       "let-values",
                              "let*-values", "letrec-syntax", "syntax-rules",
                              "if",          "cond",          "case",
                              "when",        "unless",        "and",
                              "or",          "not",           "begin",
                              "do",          "quote",         "unquote",
                              "quasiquote",  "set!",          "module",
                              "import",      "export",        "library",
                              "include",     nullptr};
const char* const BI_SCM[] = {"#t",      "#f",         "null",   "car",        "cdr",     "cons",
                              "list",    "first",      "second", "third",      "map",     "filter",
                              "fold",    "fold-right", "reduce", "apply",      "display", "newline",
                              "write",   "read",       "error",  "procedure?", "number?", "string?",
                              "symbol?", "list?",      "null?",  nullptr};

const char* const KW_CLJ[] = {
    "def",         "defn",      "defmacro",    "fn",       "let",       "loop",  "recur",
    "if",          "when",      "unless",      "cond",     "case",      "do",    "try",
    "catch",       "finally",   "throw",       "binding",  "letfn",     "var",   "quote",
    "ns",          "require",   "use",         "import",   "refer",     "in-ns", "gen-class",
    "deftype",     "defrecord", "defprotocol", "defmulti", "defmethod", "reify", "extend-protocol",
    "extend-type", nullptr};
const char* const BI_CLJ[] = {
    "nil",     "true",   "false",  "println", "print",  "pr",     "prn",      "str",
    "keyword", "symbol", "vector", "list",    "map",    "set",    "hash-map", "sorted-map",
    "assoc",   "dissoc", "get",    "update",  "conj",   "seq",    "rest",     "first",
    "last",    "count",  "apply",  "reduce",  "filter", "remove", "map?",     "vector?",
    "list?",   "set?",   "nil?",   "empty?",  nullptr};

const char* const KW_ASM[] = {
    "mov",  "add",     "sub",    "mul",    "imul", "div",  "idiv", "inc", "dec",  "neg",     "cmp",
    "test", "jmp",     "je",     "jne",    "jz",   "jnz",  "jg",   "jge", "jl",   "jle",     "ja",
    "jae",  "jb",      "jbe",    "call",   "ret",  "push", "pop",  "lea", "int",  "syscall", "nop",
    "hlt",  "cli",     "sti",    "and",    "or",   "xor",  "not",  "shl", "shr",  "sar",     "rol",
    "ror",  "section", "global", "extern", "db",   "dw",   "dd",   "dq",  "resb", "resw",    "resd",
    "resq", "equ",     "times",  "bits",   "org",  nullptr};
const char* const TY_ASM[] = {"byte", "word", "dword", "qword", "ptr", "near", "far", nullptr};
const char* const BI_ASM[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",  "rip", "r8",
                              "r9",  "r10", "r11", "r12", "r13", "r14", "r15", "eax",  "ebx", "ecx",
                              "edx", "esi", "edi", "esp", "ebp", "ax",  "bx",  "cx",   "dx",  "ah",
                              "al",  "bh",  "bl",  "ch",  "cl",  "dh",  "dl",  nullptr};

struct Spec {
    const char* const* kw;
    const char* const* bi;
    const char* const* ty;
    char comment_char;   // ';' for lisp/scheme/clj/asm
    bool hash_paren;     // #() / #{ } / #_ for Clojure/Scheme
    bool keyword_colon;  // :keyword in Clojure
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
        if (c == sp.comment_char) {
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
        if (sp.keyword_colon && c == ':' && i + 1 < n && is_ident_start(s[i + 1])) {
            size_t j = scan_while(s, i + 1, [](char ch) {
                return is_ident_cont(ch) || ch == '-' || ch == '/' || ch == '.';
            });
            emit(TokenKind::Constant, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (sp.hash_paren && c == '#') {
            // #() / #{} / #_ — emit as decorator-ish marker
            if (i + 1 < n && std::strchr("({[_'", s[i + 1])) {
                emit(TokenKind::Decorator, s.substr(i, 1));
                ++i;
                continue;
            }
        }
        if (is_digit(c) || (c == '-' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t j = scan_number(s, i + (c == '-' ? 1 : 0));
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (is_ident_start(c) || c == '+' || c == '*' || c == '/' || c == '<' || c == '>' ||
            c == '=' || c == '!' || c == '?') {
            size_t j = i + 1;
            while (j < n && (is_ident_cont(s[j]) || s[j] == '-' || s[j] == '?' || s[j] == '!' ||
                             s[j] == '*' || s[j] == '/' || s[j] == '+' || s[j] == '.'))
                ++j;
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(w, sp.kw))
                kk = TokenKind::Keyword;
            else if (sp.bi && in_keyword_set(w, sp.bi))
                kk = TokenKind::Builtin;
            else if (sp.ty && in_keyword_set(w, sp.ty))
                kk = TokenKind::BuiltinType;
            else {
                // Operator-call position heuristic: identifier right after (
                size_t back = i;
                while (back > 0 && is_space(s[back - 1]))
                    --back;
                if (back > 0 && s[back - 1] == '(')
                    kk = TokenKind::Function;
            }
            emit(kk, w);
            i = j;
            continue;
        }
        if (std::strchr("()[]{}'`,@", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

const Spec LISP = {KW_LISP, BI_LISP, nullptr, ';', false, false};
const Spec SCM = {KW_SCM, BI_SCM, nullptr, ';', true, false};
const Spec CLJ = {KW_CLJ, BI_CLJ, nullptr, ';', true, true};
const Spec ASM = {KW_ASM, BI_ASM, TY_ASM, ';', false, false};

}  // namespace

void tokenise_lisp(std::string_view s, const TokenSink& e) {
    scan(s, e, LISP);
}
void tokenise_scheme(std::string_view s, const TokenSink& e) {
    scan(s, e, SCM);
}
void tokenise_clojure(std::string_view s, const TokenSink& e) {
    scan(s, e, CLJ);
}
void tokenise_asm(std::string_view s, const TokenSink& e) {
    scan(s, e, ASM);
}

}  // namespace rcat::lang
