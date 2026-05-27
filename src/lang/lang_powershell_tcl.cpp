// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// PowerShell + Tcl tokenisers. Both very different from POSIX shell.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW_PS[] = {
    "begin",        "break",   "catch",  "class",    "continue", "data",  "define", "do",
    "dynamicparam", "else",    "elseif", "end",      "enum",     "exit",  "filter", "finally",
    "for",          "foreach", "from",   "function", "hidden",   "if",    "in",     "inlinescript",
    "param",        "process", "return", "static",   "switch",   "throw", "trap",   "try",
    "until",        "using",   "var",    "while",    "workflow", nullptr};
const char* const BI_PS[] = {"Get-ChildItem",
                             "Get-Content",
                             "Set-Content",
                             "Add-Content",
                             "Get-Item",
                             "Set-Item",
                             "Remove-Item",
                             "Copy-Item",
                             "Move-Item",
                             "New-Item",
                             "Get-Process",
                             "Stop-Process",
                             "Start-Process",
                             "Write-Output",
                             "Write-Host",
                             "Write-Error",
                             "Write-Warning",
                             "Write-Verbose",
                             "Read-Host",
                             "Out-File",
                             "Out-Null",
                             "ConvertTo-Json",
                             "ConvertFrom-Json",
                             "Select-Object",
                             "Where-Object",
                             "ForEach-Object",
                             "Sort-Object",
                             "Group-Object",
                             "Measure-Object",
                             "Invoke-Expression",
                             "Invoke-RestMethod",
                             "Invoke-WebRequest",
                             "Test-Path",
                             "Join-Path",
                             "Split-Path",
                             "Resolve-Path",
                             "$true",
                             "$false",
                             "$null",
                             nullptr};

const char* const KW_TCL[] = {
    "after",   "append",    "array",    "binary",  "break",   "case",     "catch",      "cd",
    "close",   "concat",    "continue", "dict",    "else",    "elseif",   "encoding",   "eof",
    "error",   "eval",      "exec",     "exit",    "expr",    "fblocked", "fconfigure", "fcopy",
    "file",    "fileevent", "flush",    "for",     "foreach", "format",   "gets",       "glob",
    "global",  "history",   "if",       "incr",    "info",    "interp",   "join",       "lappend",
    "lassign", "lindex",    "linsert",  "list",    "llength", "lmap",     "load",       "lrange",
    "lrepeat", "lreplace",  "lreverse", "lsearch", "lset",    "lsort",    "namespace",  "open",
    "package", "parray",    "pid",      "proc",    "puts",    "pwd",      "read",       "regexp",
    "regsub",  "rename",    "return",   "scan",    "seek",    "set",      "socket",     "source",
    "split",   "string",    "subst",    "switch",  "tell",    "throw",    "time",       "trace",
    "try",     "unknown",   "unset",    "update",  "uplevel", "upvar",    "variable",   "vwait",
    "while",   nullptr};

}  // namespace

void tokenise_powershell(std::string_view s, const TokenSink& emit) {
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
        if (c == '<' && i + 1 < n && s[i + 1] == '#') {
            // block comment <# ... #>
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == '#' && s[j + 1] == '>'))
                ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$') {
            size_t j = i + 1;
            if (j < n && s[j] == '{') {
                ++j;
                while (j < n && s[j] != '}')
                    ++j;
                if (j < n)
                    ++j;
            } else {
                j = scan_while(s, j, [](char ch) { return is_ident_cont(ch) || ch == ':'; });
            }
            emit(TokenKind::Variable, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, true);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            continue;
        }
        if (c == '-' && i + 1 < n && is_alpha(s[i + 1])) {
            size_t j = scan_ident(s, i + 1);
            emit(TokenKind::Attribute, s.substr(i, j - i));
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
            size_t j = scan_ident(s, i);
            // PowerShell allows a hyphenated cmdlet name (Verb-Noun)
            while (j < n && s[j] == '-' && j + 1 < n && is_ident_start(s[j + 1])) {
                j = scan_ident(s, j + 1);
            }
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, KW_PS))
                kk = TokenKind::Keyword;
            else if (in_keyword_set(word, BI_PS))
                kk = TokenKind::Builtin;
            else if (word.find('-') != std::string_view::npos)
                kk = TokenKind::Function;
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

void tokenise_tcl(std::string_view s, const TokenSink& emit) {
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
        if (c == '#' && at_bol) {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '$') {
            size_t j = i + 1;
            if (j < n && s[j] == '{') {
                ++j;
                while (j < n && s[j] != '}')
                    ++j;
                if (j < n)
                    ++j;
            } else {
                j = scan_while(s, j, is_ident_cont);
            }
            emit(TokenKind::Variable, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (c == '"') {
            auto r = scan_simple_string(s, i, '"', true);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        if (is_digit(c)) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = at_bol ? TokenKind::Function : TokenKind::Text;
            if (in_keyword_set(word, KW_TCL))
                kk = TokenKind::Keyword;
            emit(kk, word);
            i = j;
            at_bol = false;
            continue;
        }
        if (std::strchr("{}[]();,", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            at_bol = false;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

}  // namespace rcat::lang
