// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include "terminal.hpp"

#include <string>
#include <string_view>

namespace rcat {

struct ImageRenderResult {
    bool success = false;
    int  width_cells  = 0;
    int  height_cells = 0;
    std::string output;       // multi-line, may contain ANSI; no trailing '\n'
    std::string error;
};

// True when the build was compiled with chafa support.
[[nodiscard]] bool image_rendering_available();

// True when libcurl is linked in (i.e. --allow-web can actually fetch).
[[nodiscard]] bool web_fetch_available();

// Decode `path` (any format stb_image supports — PNG/JPG/GIF/BMP/PSD/TGA/PNM)
// and render to at most max_width x max_height terminal cells, preserving
// aspect ratio. `color` controls how many colors the canvas emits — anything
// below Ansi256 falls back to 16-color text. Always uses chafa's "symbols"
// pixel mode (half-block / quadrant chars), which is the only protocol that
// travels safely over SSH/tmux on every terminal. Returns success=false on
// any failure (file missing, unsupported format, chafa init error).
[[nodiscard]] ImageRenderResult render_image_file(std::string_view path,
                                                  int max_width_cells,
                                                  int max_height_cells,
                                                  ColorMode color);

// Same as render_image_file but reads the image from an in-memory buffer
// (e.g. an HTTP download). Caller owns `bytes` (must outlive the call).
[[nodiscard]] ImageRenderResult render_image_bytes(const unsigned char* bytes,
                                                   size_t bytes_len,
                                                   int max_width_cells,
                                                   int max_height_cells,
                                                   ColorMode color);

// Fetch a URL into memory using libcurl. Returns success=false if libcurl
// is not compiled in, the URL fails, the response is too large, or the
// transfer takes longer than timeout_seconds. Caps the body at max_bytes to
// keep a hostile or runaway server from filling memory.
struct WebFetchResult {
    bool success = false;
    std::string bytes;
    std::string error;
};
[[nodiscard]] WebFetchResult fetch_url(std::string_view url,
                                       int timeout_seconds = 10,
                                       size_t max_bytes    = 16 * 1024 * 1024);

} // namespace rcat
