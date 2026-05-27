// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "renderer.hpp"
#include "terminal.hpp"
#include "wrap.hpp"

#include <cstdio>
#include <string>

static int g_failed = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)

static rcat::RenderOptions plain_opts() {
    rcat::RenderOptions o;
    o.caps.is_tty = false;
    o.caps.columns = 60;
    o.caps.color = rcat::ColorMode::None;
    o.caps.hyperlinks = false;
    o.caps.unicode = false;
    o.plain = true;
    return o;
}

int main() {
    using rcat::display_width;
    using rcat::render_markdown;

    {
        // Heading + paragraph in plain mode
        std::string out;
        CHECK(render_markdown("# Hello\n\nworld\n", plain_opts(), out));
        CHECK(out.find("# Hello") != std::string::npos);
        CHECK(out.find("world") != std::string::npos);
    }

    {
        // Paragraph wraps to columns
        auto opts = plain_opts();
        opts.caps.columns = 20;
        std::string out;
        CHECK(render_markdown("alpha beta gamma delta epsilon zeta eta theta iota\n", opts, out));
        // Every line should be <= 20 cells (plain mode, no SGR)
        size_t pos = 0;
        while (pos < out.size()) {
            size_t nl = out.find('\n', pos);
            if (nl == std::string::npos)
                break;
            CHECK(display_width(std::string_view(out).substr(pos, nl - pos)) <= 20);
            pos = nl + 1;
        }
    }

    {
        // Unordered list with marker
        std::string out;
        CHECK(render_markdown("- one\n- two\n", plain_opts(), out));
        CHECK(out.find("* one") != std::string::npos);
        CHECK(out.find("* two") != std::string::npos);
    }

    {
        // Ordered list numbering
        std::string out;
        CHECK(render_markdown("1. a\n2. b\n3. c\n", plain_opts(), out));
        CHECK(out.find("1. a") != std::string::npos);
        CHECK(out.find("2. b") != std::string::npos);
        CHECK(out.find("3. c") != std::string::npos);
    }

    {
        // Nested list — second-level items indented further than first
        std::string out;
        CHECK(render_markdown("- outer\n    - inner\n", plain_opts(), out));
        size_t outer = out.find("* outer");
        size_t inner = out.find("* inner");
        CHECK(outer != std::string::npos);
        CHECK(inner != std::string::npos);
        // The inner marker should appear later AND be indented more.
        // Find the line start for each.
        auto line_start = [&](size_t p) {
            size_t s = out.rfind('\n', p);
            return s == std::string::npos ? 0 : s + 1;
        };
        size_t outer_col = outer - line_start(outer);
        size_t inner_col = inner - line_start(inner);
        CHECK(inner_col > outer_col);
    }

    {
        // Block quote prefix
        std::string out;
        CHECK(render_markdown("> quoted\n", plain_opts(), out));
        CHECK(out.find("| quoted") != std::string::npos);
    }

    {
        // Fenced code block — preserve content, indented 4 spaces
        std::string out;
        CHECK(render_markdown("```\nint x = 1;\n```\n", plain_opts(), out));
        CHECK(out.find("    int x = 1;") != std::string::npos);
    }

    {
        // Link in plain mode: text (url) form
        std::string out;
        CHECK(render_markdown("[click](https://example.com)\n", plain_opts(), out));
        CHECK(out.find("click") != std::string::npos);
        CHECK(out.find("https://example.com") != std::string::npos);
    }

    {
        // Task list checkboxes
        std::string out;
        CHECK(render_markdown("- [x] done\n- [ ] todo\n", plain_opts(), out));
        CHECK(out.find("[x] done") != std::string::npos);
        CHECK(out.find("[ ] todo") != std::string::npos);
    }

    {
        // Table (GFM) — should produce ASCII box in plain mode
        std::string md = "| A | B |\n"
                         "|---|---|\n"
                         "| 1 | 2 |\n";
        std::string out;
        CHECK(render_markdown(md, plain_opts(), out));
        CHECK(out.find("+") != std::string::npos);
        CHECK(out.find("| A") != std::string::npos);
        CHECK(out.find("| 1") != std::string::npos);
    }

    if (g_failed == 0)
        std::printf("test_render: OK\n");
    return g_failed == 0 ? 0 : 1;
}
