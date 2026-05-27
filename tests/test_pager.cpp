// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "pager.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_failed = 0;

#define CHECK(expr) do {                                                    \
    if (!(expr)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);\
        ++g_failed;                                                         \
    }                                                                       \
} while (0)

int main() {
    using rcat::clamp_top;
    using rcat::find_next_match;
    using rcat::find_prev_match;
    using rcat::split_visible_lines;
    using rcat::strip_ansi;

    // ---- split_visible_lines --------------------------------------------
    {
        auto v = split_visible_lines("");
        CHECK(v.empty());
    }
    {
        auto v = split_visible_lines("alpha\nbeta\ngamma\n");
        CHECK(v.size() == 3);
        CHECK(v[0] == "alpha");
        CHECK(v[1] == "beta");
        CHECK(v[2] == "gamma");
    }
    {
        // Trailing line without newline is preserved.
        auto v = split_visible_lines("one\ntwo");
        CHECK(v.size() == 2);
        CHECK(v[1] == "two");
    }
    {
        // ANSI in a line stays intact (we only split on '\n').
        std::string s = "\x1b[1mhi\x1b[0m\nplain\n";
        auto v = split_visible_lines(s);
        CHECK(v.size() == 2);
        CHECK(v[0] == "\x1b[1mhi\x1b[0m");
        CHECK(v[1] == "plain");
    }

    // ---- strip_ansi -----------------------------------------------------
    CHECK(strip_ansi("plain text") == "plain text");
    CHECK(strip_ansi("\x1b[31mred\x1b[0m") == "red");
    CHECK(strip_ansi("\x1b[1;38;5;42mfoo\x1b[0m bar") == "foo bar");
    CHECK(strip_ansi("\x1b]8;;https://example.com\x1b\\link\x1b]8;;\x1b\\")
          == "link");
    CHECK(strip_ansi("\x1b]8;;https://example.com\x07link\x1b]8;;\x07")
          == "link");

    // ---- find_next_match / find_prev_match ------------------------------
    {
        std::vector<std::string_view> lines = {
            "alpha",
            "\x1b[1mBETA\x1b[0m",
            "gamma beta",
            "delta",
        };

        // Forward, case-insensitive, finds the styled line first.
        auto m = find_next_match(lines, "beta", 0, /*case_sensitive=*/false);
        CHECK(m.has_value());
        CHECK(m && *m == 1);

        // Forward, case-sensitive, skips the upper-case styled line.
        auto m2 = find_next_match(lines, "beta", 0, /*case_sensitive=*/true);
        CHECK(m2.has_value());
        CHECK(m2 && *m2 == 2);

        // No match returns nullopt.
        CHECK(!find_next_match(lines, "zeta", 0, false).has_value());

        // Backward from the end, case-insensitive — finds line 2 (gamma beta).
        auto m3 = find_prev_match(lines, "beta", lines.size() - 1, false);
        CHECK(m3.has_value());
        CHECK(m3 && *m3 == 2);

        // Backward from line 1 only sees the styled BETA.
        auto m4 = find_prev_match(lines, "beta", 1, false);
        CHECK(m4 && *m4 == 1);

        // Empty needle never matches.
        CHECK(!find_next_match(lines, "", 0, false).has_value());

        // Empty corpus.
        std::vector<std::string_view> empty;
        CHECK(!find_next_match(empty, "x", 0, false).has_value());
        CHECK(!find_prev_match(empty, "x", 0, false).has_value());
    }

    // ---- clamp_top ------------------------------------------------------
    CHECK(clamp_top(0, 10, 0) == 0);          // empty
    CHECK(clamp_top(5, 10, 3) == 0);          // viewport > total
    CHECK(clamp_top(-3, 10, 100) == 0);       // negative top
    CHECK(clamp_top(50, 10, 100) == 50);      // mid-buffer
    CHECK(clamp_top(95, 10, 100) == 90);      // pinned to last page
    CHECK(clamp_top(1000, 5, 12) == 7);       // past EOF clamps to total - viewport
    CHECK(clamp_top(0, 0, 5) == 0);           // viewport zero (defensive)

    if (g_failed == 0) std::printf("test_pager: OK\n");
    return g_failed == 0 ? 0 : 1;
}
