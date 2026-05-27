// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "image.hpp"
#include "renderer.hpp"
#include "terminal.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#ifndef RCAT_TEST_FIXTURE_DIR
#error "RCAT_TEST_FIXTURE_DIR must be defined"
#endif

static int g_failed = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)

int main() {
    using namespace rcat;

    // Direct image render — small 16x16 PNG fixture
    {
        std::string path = std::string(RCAT_TEST_FIXTURE_DIR) + "/logo.png";
        auto r = render_image_file(path, 40, 8, ColorMode::TrueColor);
        CHECK(r.success);
        CHECK(!r.output.empty());
        // Truecolor background sequences (48;2;r;g;b) must appear.
        CHECK(r.output.find("48;2;") != std::string::npos);
        CHECK(r.width_cells > 0);
        CHECK(r.height_cells > 0);
        CHECK(r.height_cells <= 8);
    }

    // Missing file: failure, no output, no crash
    {
        auto r = render_image_file("/no/such/file.png", 40, 8, ColorMode::TrueColor);
        CHECK(!r.success);
        CHECK(r.output.empty());
    }

    // Renderer integration: local image → 48;2; in output
    {
        RenderOptions o;
        o.caps.is_tty = false;
        o.caps.columns = 60;
        o.caps.color = ColorMode::TrueColor;
        o.caps.unicode = true;
        o.doc_dir = RCAT_TEST_FIXTURE_DIR;
        o.image_max_height = 8;

        std::string out;
        CHECK(render_markdown("![logo](logo.png)\n", o, out));
        CHECK(out.find("48;2;") != std::string::npos);
    }

    // Renderer integration: remote image → label, no escape sequences
    {
        RenderOptions o;
        o.caps.is_tty = false;
        o.caps.columns = 60;
        o.caps.color = ColorMode::None;
        o.caps.unicode = false;
        o.plain = true;
        o.doc_dir = RCAT_TEST_FIXTURE_DIR;

        std::string out;
        CHECK(render_markdown("![alt](https://example.com/x.png)\n", o, out));
        CHECK(out.find("alt") != std::string::npos);
        CHECK(out.find("https://example.com/x.png") != std::string::npos);
        CHECK(out.find("\x1b[") == std::string::npos);
    }

    // --no-images respected even for local
    {
        RenderOptions o;
        o.caps.is_tty = false;
        o.caps.columns = 60;
        o.caps.color = ColorMode::TrueColor;
        o.caps.unicode = true;
        o.doc_dir = RCAT_TEST_FIXTURE_DIR;
        o.no_images = true;

        std::string out;
        CHECK(render_markdown("![logo](logo.png)\n", o, out));
        CHECK(out.find("48;2;") == std::string::npos);
        CHECK(out.find("logo.png") != std::string::npos);
    }

    // Remote URL without --allow-web: stays as text, no escape sequences for image.
    {
        RenderOptions o;
        o.caps.is_tty = false;
        o.caps.columns = 60;
        o.caps.color = ColorMode::None;
        o.caps.unicode = false;
        o.plain = true;
        o.doc_dir = RCAT_TEST_FIXTURE_DIR;
        // allow_web defaults to false

        std::string out;
        CHECK(render_markdown("![r](https://example.com/x.png)\n", o, out));
        CHECK(out.find("https://example.com/x.png") != std::string::npos);
        CHECK(out.find("48;2;") == std::string::npos);
    }

    // Remote URL with --allow-web but pointing at a non-resolvable host:
    // should NOT crash, should fall back to a label. (Uses .invalid TLD so
    // DNS reliably fails offline.)
    {
        RenderOptions o;
        o.caps.is_tty = false;
        o.caps.columns = 60;
        o.caps.color = ColorMode::TrueColor;
        o.caps.unicode = true;
        o.doc_dir = RCAT_TEST_FIXTURE_DIR;
        o.allow_web = true;
        o.web_timeout_seconds = 2;

        std::string out;
        CHECK(render_markdown("![r](https://nonexistent.invalid/x.png)\n", o, out));
        CHECK(out.find("[image: ") != std::string::npos);
        CHECK(out.find("48;2;") == std::string::npos);
    }

    if (g_failed == 0)
        std::printf("test_image: OK\n");
    return g_failed == 0 ? 0 : 1;
}
