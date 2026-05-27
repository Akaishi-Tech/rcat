// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rcat {

enum class ColorMode {
    None,       // No escape codes (NO_COLOR, dumb, non-tty)
    Ansi16,     // Basic 16 colors
    Ansi256,    // 256-color palette
    TrueColor,  // 24-bit RGB
};

struct TerminalCaps {
    bool is_tty       = false;
    int  columns      = 80;
    ColorMode color   = ColorMode::None;
    bool hyperlinks   = false;   // OSC 8
    bool unicode      = false;   // box-drawing, bullets
    bool over_ssh     = false;
};

[[nodiscard]] TerminalCaps detect_terminal_caps();

struct Style {
    bool bold      = false;
    bool italic    = false;
    bool underline = false;
    bool strike    = false;
    bool reverse   = false;
    bool dim       = false;
    int  fg_256    = -1;          // -1 = default; 0..255 otherwise
    bool use_fg_rgb = false;
    uint32_t fg_rgb = 0;          // 0xRRGGBB
};

[[nodiscard]] std::string sgr(const Style& s, ColorMode mode);
[[nodiscard]] std::string sgr_reset();
[[nodiscard]] std::string hyperlink_open(std::string_view url);
[[nodiscard]] std::string hyperlink_close();

} // namespace rcat
