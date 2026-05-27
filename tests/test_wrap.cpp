// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "wrap.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

using rcat::char_width;
using rcat::display_width;
using rcat::emit_wrapped;
using rcat::utf8_decode;

static int g_failed = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        auto _a = (a);                                                                  \
        auto _b = (b);                                                                  \
        if (!(_a == _b)) {                                                              \
            std::fprintf(stderr, "FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++g_failed;                                                                 \
        }                                                                               \
    } while (0)

int main() {
    // display_width ignores ANSI CSI
    CHECK_EQ(display_width("hello"), 5);
    CHECK_EQ(display_width("\x1b[31mhello\x1b[0m"), 5);
    CHECK_EQ(display_width("\x1b[1;31mhi\x1b[0m there"), 8);

    // display_width ignores OSC 8 (ESC ] ... ESC \)
    CHECK_EQ(display_width("\x1b]8;;https://x\x1b\\link\x1b]8;;\x1b\\"), 4);

    // utf8_decode: ASCII
    {
        uint32_t cp;
        int n = utf8_decode("A", 0, cp);
        CHECK_EQ(n, 1);
        CHECK_EQ(cp, 0x41u);
    }
    // utf8_decode: 3-byte (中)
    {
        uint32_t cp;
        int n = utf8_decode("\xE4\xB8\xAD", 0, cp);
        CHECK_EQ(n, 3);
        CHECK_EQ(cp, 0x4E2Du);
        CHECK(char_width(cp) >= 1);  // typically 2, depends on wcwidth
    }

    // emit_wrapped: simple wrap
    {
        std::string out;
        emit_wrapped(out, "the quick brown fox jumps over the lazy dog", "", "", 15);
        // Each line should be <= 15 cells
        size_t pos = 0;
        while (pos < out.size()) {
            size_t nl = out.find('\n', pos);
            if (nl == std::string::npos)
                break;
            CHECK(display_width(std::string_view(out).substr(pos, nl - pos)) <= 15);
            pos = nl + 1;
        }
        CHECK(out.find("the quick") == 0);
    }

    // emit_wrapped: hanging indent
    {
        std::string out;
        emit_wrapped(out, "one two three four five six seven", "* ", "  ", 12);
        CHECK(out.rfind("* ", 0) == 0);
        // Second line should start with "  "
        size_t nl = out.find('\n');
        CHECK(nl != std::string::npos);
        CHECK(out.substr(nl + 1, 2) == "  ");
    }

    // emit_wrapped: respects embedded SGR (escape sequences are zero-width)
    {
        std::string out;
        std::string in = "\x1b[1mbold word\x1b[0m and tail";
        emit_wrapped(out, in, "", "", 15);
        // No line should overflow when counting visible cells
        size_t pos = 0;
        while (pos < out.size()) {
            size_t nl = out.find('\n', pos);
            if (nl == std::string::npos)
                break;
            CHECK(display_width(std::string_view(out).substr(pos, nl - pos)) <= 15);
            pos = nl + 1;
        }
    }

    // emit_wrapped: hard newline in input forces a break
    {
        std::string out;
        emit_wrapped(out, "alpha\nbeta", "", "", 40);
        // Should produce exactly two lines
        int newlines = 0;
        for (char c : out)
            if (c == '\n')
                ++newlines;
        CHECK_EQ(newlines, 2);
        CHECK(out.find("alpha\n") != std::string::npos);
        CHECK(out.find("beta\n") != std::string::npos);
    }

    if (g_failed == 0)
        std::printf("test_wrap: OK\n");
    return g_failed == 0 ? 0 : 1;
}
