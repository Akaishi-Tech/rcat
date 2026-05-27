// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "highlight.hpp"
#include "i18n.hpp"
#include "rcat_version.hpp"
#include "renderer.hpp"
#include "terminal.hpp"

#include <argparse/argparse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Read an entire file (or stdin when `path == "-"`) into a string.
// Returns std::nullopt on I/O failure and writes a diagnostic to stderr.
[[nodiscard]] std::optional<std::string> read_file(const fs::path& path) {
    if (path == "-") {
        std::string out{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>{}};
        return out;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "rcat: %s: %s\n", path.string().c_str(), std::strerror(errno));
        return std::nullopt;
    }
    std::string out{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{}};
    if (in.bad()) {
        std::fprintf(stderr, "rcat: %s: %s\n", path.string().c_str(), std::strerror(errno));
        return std::nullopt;
    }
    return out;
}

struct ColorChoice {
    rcat::ColorMode mode = rcat::ColorMode::None;
    bool is_auto = false;
};

// "auto" -> {*, true}; otherwise {mode, false}; std::nullopt for an invalid value.
[[nodiscard]] std::optional<ColorChoice> parse_color_mode(std::string_view s) {
    if (s == "auto")
        return ColorChoice{{}, true};
    if (s == "none")
        return ColorChoice{rcat::ColorMode::None, false};
    if (s == "16")
        return ColorChoice{rcat::ColorMode::Ansi16, false};
    if (s == "256")
        return ColorChoice{rcat::ColorMode::Ansi256, false};
    if (s == "truecolor" || s == "24bit")
        return ColorChoice{rcat::ColorMode::TrueColor, false};
    return std::nullopt;
}

[[nodiscard]] std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// True when the file should go through the Markdown renderer rather than
// the standalone syntax highlighter.
[[nodiscard]] bool looks_markdown(const fs::path& path, std::string_view explicit_lang) {
    if (explicit_lang == "md" || explicit_lang == "markdown")
        return true;
    if (!explicit_lang.empty())
        return false;

    // stdin or extension-less file: default to Markdown.
    if (!path.has_extension())
        return true;

    static constexpr std::array<std::string_view, 5> kMdExts{
        "md", "markdown", "mdown", "mkd", "mkdn",
    };
    std::string ext = to_lower(path.extension().string());
    if (!ext.empty() && ext.front() == '.')
        ext.erase(0, 1);
    if (std::find(kMdExts.begin(), kMdExts.end(), ext) != kMdExts.end())
        return true;

    // If detect_language recognises the extension, treat as source code instead.
    rcat::Language guess = rcat::detect_language({}, path.string(), {});
    return guess == rcat::Language::PlainText;
}

// Every info-string alias the highlighter recognises; printed by `--lang list`.
constexpr std::array<std::string_view, 75> kLangAliases{
    "c",          "cpp",     "java",     "javascript", "typescript", "csharp",     "go",
    "rust",       "kotlin",  "scala",    "swift",      "dart",       "objc",       "php",
    "groovy",     "zig",     "v",        "nim",        "crystal",    "d",          "python",
    "ruby",       "perl",    "r",        "julia",      "sh",         "bash",       "powershell",
    "tcl",        "sql",     "lua",      "haskell",    "elm",        "ada",        "lisp",
    "scheme",     "clojure", "asm",      "ocaml",      "fsharp",     "elixir",     "erlang",
    "html",       "xml",     "svg",      "css",        "scss",       "less",       "json",
    "json5",      "yaml",    "toml",     "ini",        "env",        "properties", "makefile",
    "dockerfile", "cmake",   "nginx",    "apache",     "diff",       "patch",      "gitconfig",
    "gitignore",  "vim",     "markdown", "latex",
};

[[nodiscard]] fs::path doc_dir_of(const fs::path& path) {
    if (path == "-")
        return fs::path{"."};
    fs::path parent = path.parent_path();
    return parent.empty() ? fs::path{"."} : parent;
}

void write_to_stdout(std::string_view buf) {
    std::fwrite(buf.data(), 1, buf.size(), stdout);
}

}  // namespace

int main(int argc, char** argv) {
    rcat::init_i18n();

    argparse::ArgumentParser app("rcat", rcat::kVersion, argparse::default_arguments::help);
    app.add_description(
        _("Render Markdown — and many other source/config files — in the terminal."));
    app.add_epilog(_("Reads from stdin if no FILE is given (or FILE is '-').\n"
                     "Image paths are resolved relative to each FILE's directory.\n"
                     "Non-Markdown files (detected by extension or --lang) are rendered\n"
                     "with syntax highlighting instead of through the Markdown parser."));

    app.add_argument("-V", "--version")
        .help(_("show version and exit"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("-c", "--columns")
        .help(_("wrap to N columns (default: terminal width)"))
        .metavar("N")
        .scan<'i', int>();

    app.add_argument("-p", "--plain")
        .help(_("no styling at all (just wrapped text)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--no-color")
        .help(_("disable color (keep other styling/Unicode)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--force-color")
        .help(_("emit colors even on non-TTY (e.g. piping to less -R)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--no-hyperlinks")
        .help(_("disable OSC 8 hyperlinks even if supported"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--no-images")
        .help(_("don't render inline images"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--allow-web")
        .help(_("download and render remote http(s) image URLs (off by default)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--web-timeout")
        .help(_("seconds to wait per remote image fetch (default 10)"))
        .metavar("SEC")
        .scan<'i', int>();

    app.add_argument("--image-height")
        .help(_("cap image height to N rows (default 20)"))
        .metavar("N")
        .scan<'i', int>();

    app.add_argument("--ascii")
        .help(_("force ASCII-only output (no box-drawing/bullets)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("--color")
        .help(_("force color mode: auto|none|16|256|truecolor"))
        .metavar("MODE")
        .default_value(std::string("auto"));

    app.add_argument("--lang")
        .help(_("force source language (e.g. python, json, cpp); use 'md' for "
                "Markdown, 'list' to print every language"))
        .metavar("LANG")
        .default_value(std::string());

    app.add_argument("files")
        .help(_("file(s); use '-' for stdin. Markdown by default, or any source/config file"))
        .metavar("FILE")
        .remaining();

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "rcat: %s\n%s\n", e.what(),
                     _("Try 'rcat --help' for more information."));
        return 2;
    }

    if (app.get<bool>("--version")) {
        std::printf("rcat %s\n", rcat::kVersion);
        return 0;
    }

    rcat::RenderOptions opts;
    opts.caps = rcat::detect_terminal_caps();

    if (auto n = app.present<int>("--columns")) {
        if (*n < 10) {
            std::fprintf(stderr, "rcat: %s\n", _("--columns must be >= 10"));
            return 2;
        }
        opts.caps.columns = *n;
    }
    if (auto n = app.present<int>("--image-height")) {
        if (*n < 1) {
            std::fprintf(stderr, "rcat: %s\n", _("--image-height must be >= 1"));
            return 2;
        }
        opts.image_max_height = *n;
    }

    opts.plain = app.get<bool>("--plain");
    opts.no_hyperlinks = app.get<bool>("--no-hyperlinks");
    opts.no_images = app.get<bool>("--no-images");
    opts.allow_web = app.get<bool>("--allow-web");

    if (auto n = app.present<int>("--web-timeout")) {
        if (*n < 1) {
            std::fprintf(stderr, "rcat: %s\n", _("--web-timeout must be >= 1"));
            return 2;
        }
        opts.web_timeout_seconds = *n;
    }

    if (app.get<bool>("--ascii"))
        opts.caps.unicode = false;
    if (app.get<bool>("--no-color"))
        opts.caps.color = rcat::ColorMode::None;

    if (auto choice = parse_color_mode(app.get<std::string>("--color"))) {
        if (!choice->is_auto)
            opts.caps.color = choice->mode;
    } else {
        std::fprintf(stderr, "rcat: %s\n", _("invalid --color value"));
        return 2;
    }

    if (app.get<bool>("--force-color") && opts.caps.color == rcat::ColorMode::None) {
        opts.caps.color = rcat::ColorMode::Ansi256;
    }
    if (opts.caps.color == rcat::ColorMode::None) {
        opts.caps.hyperlinks = false;
    }

    std::vector<std::string> files;
    try {
        files = app.get<std::vector<std::string>>("files");
    } catch (const std::logic_error&) {
        // No positional files provided.
    }
    if (files.empty())
        files.emplace_back("-");

    const std::string forced_lang = app.get<std::string>("--lang");
    if (forced_lang == "list") {
        for (std::string_view alias : kLangAliases) {
            std::fwrite(alias.data(), 1, alias.size(), stdout);
            std::putc('\n', stdout);
        }
        return 0;
    }

    int exit_code = 0;
    for (const auto& f : files) {
        const fs::path path{f};
        const auto src_opt = read_file(path);
        if (!src_opt) {
            exit_code = 1;
            continue;
        }
        const std::string& src = *src_opt;

        opts.doc_dir = doc_dir_of(path).string();

        if (looks_markdown(path, forced_lang)) {
            std::string rendered;
            if (!rcat::render_markdown(src, opts, rendered)) {
                std::fprintf(stderr, "rcat: %s: %s\n", path.string().c_str(), _("parse error"));
                exit_code = 1;
                continue;
            }
            write_to_stdout(rendered);
        } else {
            // Highlight as a source/config file.
            const auto nl = src.find('\n');
            const std::string_view first_line{src.data(),
                                              nl == std::string::npos ? src.size() : nl};
            const std::string filename = (path == "-") ? std::string{} : path.string();
            const rcat::Language lang = rcat::detect_language(forced_lang, filename, first_line);
            const rcat::ColorMode cm = opts.plain ? rcat::ColorMode::None : opts.caps.color;
            std::string rendered;
            rcat::highlight_to_stream(src, lang, cm, /*prefix*/ "", rendered);
            write_to_stdout(rendered);
        }
    }
    return exit_code;
}
