// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "wrap.hpp"

#include <wchar.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace rcat {

int utf8_decode(std::string_view s, size_t i, uint32_t& cp) {
    if (i >= s.size()) { cp = 0; return 0; }
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { cp = c; return 1; }

    int len;
    if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
    else                          { cp = 0xFFFD;  return 1; }

    if (i + static_cast<size_t>(len) > s.size()) { cp = 0xFFFD; return 1; }
    for (int k = 1; k < len; ++k) {
        unsigned char x = static_cast<unsigned char>(s[i + k]);
        if ((x & 0xC0) != 0x80) { cp = 0xFFFD; return 1; }
        cp = (cp << 6) | (x & 0x3F);
    }
    return len;
}

int char_width(uint32_t cp) {
    if (cp == 0) return 0;
    if (cp < 0x20)               return 0;
    if (cp >= 0x7F && cp < 0xA0) return 0;
    int w = wcwidth(static_cast<wchar_t>(cp));
    return w < 0 ? 1 : w;
}

namespace {

// Advance past one ANSI escape sequence starting at s[i] (s[i] == ESC).
// Returns the new index. Handles CSI (ESC [ ... letter) and OSC
// (ESC ] ... BEL  |  ESC ] ... ESC \).
size_t skip_escape(std::string_view s, size_t i) {
    if (i >= s.size() || s[i] != '\x1b') return i + 1;
    ++i;
    if (i >= s.size()) return i;

    char c = s[i];
    if (c == '[') {
        ++i;
        while (i < s.size()) {
            unsigned char ch = static_cast<unsigned char>(s[i]);
            if (ch >= 0x40 && ch <= 0x7E) { ++i; break; }
            ++i;
        }
    } else if (c == ']') {
        ++i;
        while (i < s.size()) {
            if (s[i] == '\x07') { ++i; break; }
            if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '\\') {
                i += 2;
                break;
            }
            ++i;
        }
    } else {
        ++i; // single-char escape (BEL, ST, etc.)
    }
    return i;
}

} // namespace

int display_width(std::string_view s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\x1b') { i = skip_escape(s, i); continue; }
        uint32_t cp;
        int n = utf8_decode(s, i, cp);
        w += char_width(cp);
        i += n;
    }
    return w;
}

void emit_wrapped(std::string& out,
                  std::string_view text,
                  std::string_view first_prefix,
                  std::string_view cont_prefix,
                  int columns,
                  std::string_view style_open) {
    int first_pw = display_width(first_prefix);
    int cont_pw  = display_width(cont_prefix);
    int first_avail = std::max(1, columns - first_pw);
    int cont_avail  = std::max(1, columns - cont_pw);

    std::string line;
    int line_w = 0;
    bool first_line = true;

    auto flush = [&]() {
        out.append(first_line ? first_prefix : cont_prefix);
        if (!first_line && !style_open.empty()) out.append(style_open);
        out.append(line);
        out.push_back('\n');
        line.clear();
        line_w = 0;
        first_line = false;
    };

    size_t i = 0;
    while (i < text.size()) {
        // Hard line break — emit current line and continue
        if (text[i] == '\n') {
            flush();
            ++i;
            continue;
        }
        // Skip runs of spaces between words (do not start lines with spaces)
        if (text[i] == ' ') {
            ++i;
            continue;
        }

        // Collect a word: a run of non-space non-newline cells (escape
        // sequences embedded inside the word are zero-width).
        size_t word_start = i;
        int    word_w     = 0;
        while (i < text.size() && text[i] != ' ' && text[i] != '\n') {
            if (text[i] == '\x1b') {
                i = skip_escape(text, i);
            } else {
                uint32_t cp;
                int n = utf8_decode(text, i, cp);
                word_w += char_width(cp);
                i += n;
            }
        }
        std::string_view word(text.data() + word_start, i - word_start);

        int avail = first_line ? first_avail : cont_avail;
        if (line_w == 0) {
            line.append(word);
            line_w = word_w;
        } else if (line_w + 1 + word_w <= avail) {
            line.push_back(' ');
            line.append(word);
            line_w += 1 + word_w;
        } else {
            flush();
            line.append(word);
            line_w = word_w;
        }
    }

    if (!line.empty() || first_line) flush();
}

} // namespace rcat
