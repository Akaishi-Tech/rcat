// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// Ruby tokeniser. Handles symbols (:foo), instance/global vars,
// string interpolation, and Ruby's many keywords.

#include "lang_common.hpp"

#include <cstring>

namespace rcat::lang {

namespace {

const char* const KW[] = {
    "BEGIN","END","alias","and","begin","break","case","class","def","defined?",
    "do","else","elsif","end","ensure","false","for","if","in","module","next",
    "nil","not","or","redo","rescue","retry","return","self","super","then",
    "true","undef","unless","until","when","while","yield","require","require_relative",
    "include","extend","attr_accessor","attr_reader","attr_writer","raise",
    nullptr};
const char* const TY[] = {
    "Array","Hash","String","Symbol","Integer","Float","Numeric","TrueClass",
    "FalseClass","NilClass","Object","Class","Module","Range","Proc","Lambda",
    nullptr};
const char* const BI[] = {
    "true","false","nil","self","puts","print","p","pp","gets","loop","lambda",
    "proc","new","to_s","to_i","to_f","to_a","to_h","inspect","class","is_a?",
    "kind_of?","instance_of?","respond_to?","send","__method__","__send__",
    nullptr};

} // namespace

void tokenise_ruby(std::string_view s, const TokenSink& emit) {
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
        // =begin/=end block comments at BOL
        if (c == '=' && (i == 0 || s[i - 1] == '\n')
            && starts_with(s, i, "=begin")) {
            size_t j = i;
            while (j < n) {
                if (s[j] == '\n' && j + 1 < n && starts_with(s, j + 1, "=end")) {
                    j = j + 1;
                    while (j < n && s[j] != '\n') ++j;
                    break;
                }
                ++j;
            }
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            continue;
        }
        // :symbol
        if (c == ':' && i + 1 < n && is_ident_start(s[i + 1])) {
            size_t j = scan_ident(s, i + 1);
            emit(TokenKind::Constant, s.substr(i, j - i));
            i = j;
            continue;
        }
        // @var, @@cvar, $gvar
        if (c == '@' || c == '$') {
            size_t j = i + 1;
            if (c == '@' && j < n && s[j] == '@') ++j;
            if (j < n && is_ident_start(s[j])) {
                j = scan_ident(s, j);
                emit(TokenKind::Variable, s.substr(i, j - i));
                i = j;
                continue;
            }
        }
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, false);
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
            // Ruby identifiers can end with ? or !
            if (j < n && (s[j] == '?' || s[j] == '!')) ++j;
            std::string_view word = s.substr(i, j - i);
            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, KW))      kk = TokenKind::Keyword;
            else if (in_keyword_set(word, TY)) kk = TokenKind::BuiltinType;
            else if (in_keyword_set(word, BI)) kk = TokenKind::Builtin;
            else if (word.size() > 0 && word[0] >= 'A' && word[0] <= 'Z') {
                kk = TokenKind::Constant;
            } else {
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

} // namespace rcat::lang
