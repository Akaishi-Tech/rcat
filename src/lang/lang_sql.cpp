// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// SQL — keyword-rich, comment styles -- and /* … */.

#include "lang_common.hpp"

#include <cctype>
#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {"ABORT",
                          "ABSOLUTE",
                          "ACTION",
                          "ADD",
                          "ALL",
                          "ALLOCATE",
                          "ALTER",
                          "ANALYZE",
                          "AND",
                          "ANY",
                          "ARRAY",
                          "AS",
                          "ASC",
                          "ASENSITIVE",
                          "ASYMMETRIC",
                          "AT",
                          "ATOMIC",
                          "AUTHORIZATION",
                          "BEGIN",
                          "BETWEEN",
                          "BIGINT",
                          "BINARY",
                          "BIT",
                          "BLOB",
                          "BOOLEAN",
                          "BOTH",
                          "BY",
                          "CALL",
                          "CASCADE",
                          "CASCADED",
                          "CASE",
                          "CAST",
                          "CHAR",
                          "CHARACTER",
                          "CHECK",
                          "CLOB",
                          "CLOSE",
                          "COLLATE",
                          "COLUMN",
                          "COMMIT",
                          "CONNECT",
                          "CONSTRAINT",
                          "CONTINUE",
                          "CORRESPONDING",
                          "CREATE",
                          "CROSS",
                          "CUBE",
                          "CURRENT",
                          "CURSOR",
                          "DATE",
                          "DAY",
                          "DEALLOCATE",
                          "DEC",
                          "DECIMAL",
                          "DECLARE",
                          "DEFAULT",
                          "DELETE",
                          "DESC",
                          "DESCRIBE",
                          "DETERMINISTIC",
                          "DISCONNECT",
                          "DISTINCT",
                          "DO",
                          "DOUBLE",
                          "DROP",
                          "DYNAMIC",
                          "EACH",
                          "ELEMENT",
                          "ELSE",
                          "END",
                          "ESCAPE",
                          "EXCEPT",
                          "EXEC",
                          "EXECUTE",
                          "EXISTS",
                          "EXIT",
                          "EXTERNAL",
                          "FALSE",
                          "FETCH",
                          "FILTER",
                          "FLOAT",
                          "FOR",
                          "FOREIGN",
                          "FREE",
                          "FROM",
                          "FULL",
                          "FUNCTION",
                          "GET",
                          "GLOBAL",
                          "GRANT",
                          "GROUP",
                          "GROUPING",
                          "HAVING",
                          "HOLD",
                          "HOUR",
                          "IDENTITY",
                          "IF",
                          "IMMEDIATE",
                          "IN",
                          "INDEX",
                          "INDICATOR",
                          "INNER",
                          "INOUT",
                          "INSENSITIVE",
                          "INSERT",
                          "INT",
                          "INTEGER",
                          "INTERSECT",
                          "INTERVAL",
                          "INTO",
                          "IS",
                          "ISOLATION",
                          "JOIN",
                          "KEY",
                          "LANGUAGE",
                          "LARGE",
                          "LATERAL",
                          "LEADING",
                          "LEAVE",
                          "LEFT",
                          "LIKE",
                          "LIMIT",
                          "LOCAL",
                          "LOCALTIME",
                          "LOCALTIMESTAMP",
                          "LOOP",
                          "MATCH",
                          "MEMBER",
                          "MERGE",
                          "METHOD",
                          "MINUTE",
                          "MODIFIES",
                          "MODULE",
                          "MONTH",
                          "MULTISET",
                          "NATIONAL",
                          "NATURAL",
                          "NCHAR",
                          "NCLOB",
                          "NEW",
                          "NEXT",
                          "NO",
                          "NONE",
                          "NOT",
                          "NULL",
                          "NUMERIC",
                          "OF",
                          "OLD",
                          "ON",
                          "ONLY",
                          "OPEN",
                          "OR",
                          "ORDER",
                          "OUT",
                          "OUTER",
                          "OUTPUT",
                          "OVER",
                          "OVERLAPS",
                          "PARAMETER",
                          "PARTITION",
                          "PRECISION",
                          "PREPARE",
                          "PRIMARY",
                          "PROCEDURE",
                          "RANGE",
                          "READS",
                          "REAL",
                          "RECURSIVE",
                          "REF",
                          "REFERENCES",
                          "REFERENCING",
                          "RELEASE",
                          "REPEAT",
                          "RESIGNAL",
                          "RESULT",
                          "RETURN",
                          "RETURNS",
                          "REVOKE",
                          "RIGHT",
                          "ROLLBACK",
                          "ROLLUP",
                          "ROUTINE",
                          "ROW",
                          "ROWS",
                          "SAVEPOINT",
                          "SCROLL",
                          "SEARCH",
                          "SECOND",
                          "SELECT",
                          "SENSITIVE",
                          "SESSION_USER",
                          "SET",
                          "SIGNAL",
                          "SIMILAR",
                          "SMALLINT",
                          "SOME",
                          "SPECIFIC",
                          "SPECIFICTYPE",
                          "SQL",
                          "SQLEXCEPTION",
                          "SQLSTATE",
                          "SQLWARNING",
                          "START",
                          "STATIC",
                          "SUBMULTISET",
                          "SYMMETRIC",
                          "SYSTEM",
                          "SYSTEM_USER",
                          "TABLE",
                          "TABLESAMPLE",
                          "THEN",
                          "TIME",
                          "TIMESTAMP",
                          "TIMEZONE_HOUR",
                          "TIMEZONE_MINUTE",
                          "TO",
                          "TRAILING",
                          "TRANSLATION",
                          "TREAT",
                          "TRIGGER",
                          "TRUE",
                          "UNDO",
                          "UNION",
                          "UNIQUE",
                          "UNKNOWN",
                          "UNTIL",
                          "UPDATE",
                          "USER",
                          "USING",
                          "VALUE",
                          "VALUES",
                          "VARCHAR",
                          "VARYING",
                          "VIEW",
                          "WHEN",
                          "WHENEVER",
                          "WHERE",
                          "WHILE",
                          "WINDOW",
                          "WITH",
                          "WITHIN",
                          "WITHOUT",
                          "YEAR",
                          "TEXT",
                          "SERIAL",
                          "BLOB",
                          "BYTEA",
                          "UUID",
                          "JSON",
                          "JSONB",
                          nullptr};

bool keyword_case_insensitive(std::string_view w) {
    char buf[64];
    if (w.size() >= sizeof(buf))
        return false;
    for (size_t k = 0; k < w.size(); ++k) {
        buf[k] = (char)std::toupper((unsigned char)w[k]);
    }
    buf[w.size()] = 0;
    for (const char* const* p = KW; *p; ++p) {
        if (std::strcmp(*p, buf) == 0)
            return true;
    }
    return false;
}

}  // namespace

void tokenise_sql(std::string_view s, const TokenSink& emit) {
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
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '/'))
                ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '\'') {
            // SQL string: '' is an escape, no backslash escapes (most dialects)
            size_t j = i + 1;
            while (j < n) {
                if (s[j] == '\'') {
                    if (j + 1 < n && s[j + 1] == '\'') {
                        j += 2;
                        continue;
                    }
                    ++j;
                    break;
                }
                if (s[j] == '\n')
                    break;
                ++j;
            }
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '"' || c == '`') {
            auto r = scan_simple_string(s, i, c, false);
            emit(TokenKind::Attribute, s.substr(i, r.end - i));
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
            std::string_view w = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (keyword_case_insensitive(w))
                kk = TokenKind::Keyword;
            emit(kk, w);
            i = j;
            continue;
        }
        if (std::strchr("(),;", c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
    }
}

}  // namespace rcat::lang
