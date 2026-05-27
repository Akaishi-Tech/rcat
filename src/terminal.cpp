// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "terminal.hpp"

#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>

namespace rcat {

namespace {

const char* env_or_empty(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

bool contains(const char* hay, const char* needle) {
    return std::strstr(hay, needle) != nullptr;
}

bool env_set(const char* name) {
    const char* v = std::getenv(name);
    return v && *v;
}

}  // namespace

TerminalCaps detect_terminal_caps() {
    TerminalCaps c;
    c.is_tty = isatty(STDOUT_FILENO) != 0;

    if (c.is_tty) {
        struct winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
            c.columns = ws.ws_col;
        }
    }
    if (const char* cols = std::getenv("COLUMNS")) {
        int n = std::atoi(cols);
        if (n > 0)
            c.columns = n;
    }
    if (c.columns < 20)
        c.columns = 20;

    const char* term = env_or_empty("TERM");
    const char* colorterm = env_or_empty("COLORTERM");
    const char* term_program = env_or_empty("TERM_PROGRAM");
    const char* lang = env_or_empty("LANG");

    c.over_ssh = env_set("SSH_TTY") || env_set("SSH_CONNECTION");

    bool dumb = std::strcmp(term, "dumb") == 0 || term[0] == '\0';

    if (env_set("NO_COLOR") || dumb || !c.is_tty) {
        c.color = ColorMode::None;
    } else if (std::strcmp(colorterm, "truecolor") == 0 || std::strcmp(colorterm, "24bit") == 0) {
        c.color = ColorMode::TrueColor;
    } else if (contains(term, "256color")) {
        c.color = ColorMode::Ansi256;
    } else if (contains(term, "color") || std::strcmp(term, "xterm") == 0 ||
               std::strcmp(term, "screen") == 0 || std::strcmp(term, "tmux") == 0 ||
               std::strcmp(term, "linux") == 0 || std::strcmp(term, "rxvt") == 0 ||
               std::strcmp(term, "vt100") == 0 || std::strcmp(term, "vt220") == 0 ||
               std::strcmp(term, "ansi") == 0) {
        c.color = ColorMode::Ansi16;
    } else {
        c.color = ColorMode::None;
    }

    // OSC 8 hyperlinks: enable only when a known-good signal is present.
    // Safe over SSH (just escape codes); be conservative inside tmux/screen
    // because they may swallow OSC sequences without DCS passthrough.
    if (c.is_tty && c.color != ColorMode::None && std::strcmp(term, "screen") != 0 &&
        std::strncmp(term, "tmux", 4) != 0) {
        if (env_set("VTE_VERSION") || env_set("KITTY_WINDOW_ID") || env_set("WT_SESSION") ||
            env_set("KONSOLE_VERSION") || env_set("ALACRITTY_LOG") ||
            env_set("WEZTERM_EXECUTABLE") || env_set("GHOSTTY_RESOURCES_DIR") ||
            contains(term_program, "iTerm") || contains(term_program, "WezTerm") ||
            contains(term_program, "Apple_Terminal") || contains(term_program, "vscode") ||
            contains(term, "kitty") || contains(term, "foot") || contains(term, "alacritty") ||
            contains(term, "wezterm") || contains(term, "ghostty")) {
            c.hyperlinks = true;
        }
    }

    c.unicode = contains(lang, "UTF-8") || contains(lang, "utf8") ||
                contains(env_or_empty("LC_ALL"), "UTF-8") ||
                contains(env_or_empty("LC_CTYPE"), "UTF-8");

    return c;
}

std::string sgr_reset() {
    return "\x1b[0m";
}

std::string sgr(const Style& s, ColorMode mode) {
    if (mode == ColorMode::None)
        return "";
    std::string out = "\x1b[0";
    if (s.bold)
        out += ";1";
    if (s.dim)
        out += ";2";
    if (s.italic)
        out += ";3";
    if (s.underline)
        out += ";4";
    if (s.reverse)
        out += ";7";
    if (s.strike)
        out += ";9";

    if (mode == ColorMode::TrueColor && s.use_fg_rgb) {
        unsigned r = (s.fg_rgb >> 16) & 0xff;
        unsigned g = (s.fg_rgb >> 8) & 0xff;
        unsigned b = s.fg_rgb & 0xff;
        out += ";38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b);
    } else if ((mode == ColorMode::Ansi256 || mode == ColorMode::TrueColor) && s.fg_256 >= 0) {
        out += ";38;5;" + std::to_string(s.fg_256);
    } else if (mode == ColorMode::Ansi16 && s.fg_256 >= 0) {
        int c = s.fg_256;
        if (c < 16) {
            int base = c < 8 ? 30 + c : 90 + (c - 8);
            out += ";" + std::to_string(base);
        } else if (c < 232) {
            int idx = (c - 16) % 8;
            out += ";" + std::to_string(90 + idx);
        }
    }
    out += "m";
    return out;
}

std::string hyperlink_open(std::string_view url) {
    std::string s = "\x1b]8;;";
    s.append(url.data(), url.size());
    s += "\x1b\\";
    return s;
}

std::string hyperlink_close() {
    return "\x1b]8;;\x1b\\";
}

}  // namespace rcat
