// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include "terminal.hpp"

#include <string>
#include <string_view>

namespace rcat {

struct RenderOptions {
    TerminalCaps caps;
    bool force_color    = false;  // emit colors even if caps say none
    bool no_hyperlinks  = false;  // disable OSC 8 even if supported
    bool plain          = false;  // strip styling entirely
    bool no_images      = false;  // skip inline image rendering
    bool allow_web      = false;  // download http(s) image URLs (requires libcurl)
    std::string doc_dir;          // directory used to resolve relative image paths
    int  image_max_height = 20;   // upper bound on rendered image height in cells
    int  web_timeout_seconds = 10;
    size_t web_max_bytes  = 16 * 1024 * 1024;
};

// Render Markdown source to `out`. Returns false on parse error.
[[nodiscard]] bool render_markdown(std::string_view source,
                                   const RenderOptions& opts,
                                   std::string& out);

} // namespace rcat
