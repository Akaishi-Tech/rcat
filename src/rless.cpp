// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "highlight.hpp"
#include "i18n.hpp"
#include "pager.hpp"
#include "rcat_version.hpp"
#include "renderer.hpp"
#include "terminal.hpp"

#include <argparse/argparse.hpp>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::optional<std::string> read_file(const fs::path& path) {
    if (path == "-") {
        std::string out{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>{}};
        return out;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "rless: %s: %s\n", path.string().c_str(), std::strerror(errno));
        return std::nullopt;
    }
    std::string out{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{}};
    if (in.bad()) {
        std::fprintf(stderr, "rless: %s: %s\n", path.string().c_str(), std::strerror(errno));
        return std::nullopt;
    }
    return out;
}

struct ColorChoice {
    rcat::ColorMode mode = rcat::ColorMode::None;
    bool is_auto = false;
};

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

[[nodiscard]] bool looks_markdown(const fs::path& path, std::string_view explicit_lang) {
    if (explicit_lang == "md" || explicit_lang == "markdown")
        return true;
    if (!explicit_lang.empty())
        return false;
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

    rcat::Language guess = rcat::detect_language({}, path.string(), {});
    return guess == rcat::Language::PlainText;
}

[[nodiscard]] fs::path doc_dir_of(const fs::path& path) {
    if (path == "-")
        return fs::path{"."};
    fs::path parent = path.parent_path();
    return parent.empty() ? fs::path{"."} : parent;
}

// Read terminal width/color caps the way rcat does, but probe /dev/tty
// instead of stdout so the values are right when the user redirects with
// `rless foo.md > out` — though we still fall back to the env / 80x24.
[[nodiscard]] rcat::TerminalCaps probe_tty_caps() {
    rcat::TerminalCaps caps = rcat::detect_terminal_caps();
    int tfd = ::open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (tfd >= 0) {
        struct winsize ws{};
        if (ioctl(tfd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
            caps.columns = ws.ws_col;
        }
        // Treat the existence of a real controlling tty as "is_tty",
        // so the renderer emits styled output even when stdout is the
        // pager's pipe target.
        caps.is_tty = true;
        ::close(tfd);
    }
    if (caps.columns < 20)
        caps.columns = 20;
    return caps;
}

// One file → one rendered string. `cols` lets the pager re-call us at a
// different width on terminal resize.
std::string render_one(const std::string& src, const fs::path& path, std::string_view forced_lang,
                       rcat::RenderOptions opts, int cols) {
    opts.caps.columns = cols < 20 ? 20 : cols;
    opts.doc_dir = doc_dir_of(path).string();

    if (looks_markdown(path, forced_lang)) {
        std::string out;
        if (!rcat::render_markdown(src, opts, out)) {
            // Parse error: fall back to raw source so the user can still
            // read it. The error itself is reported by the caller.
            return src;
        }
        return out;
    }

    const auto nl = src.find('\n');
    const std::string_view first_line{src.data(), nl == std::string::npos ? src.size() : nl};
    const std::string filename = (path == "-") ? std::string{} : path.string();
    const rcat::Language lang = rcat::detect_language(forced_lang, filename, first_line);
    const rcat::ColorMode cm = opts.plain ? rcat::ColorMode::None : opts.caps.color;
    std::string out;
    rcat::highlight_to_stream(src, lang, cm, /*prefix*/ "", out);
    return out;
}

void write_to_stdout(std::string_view buf) {
    std::fwrite(buf.data(), 1, buf.size(), stdout);
}

}  // namespace

int main(int argc, char** argv) {
    rcat::init_i18n();

    argparse::ArgumentParser app("rless", rcat::kVersion, argparse::default_arguments::help);
    app.add_description(_("Page through Markdown — and many other source/config files — "
                          "in the terminal."));
    app.add_epilog(_("Reads from stdin if no FILE is given (or FILE is '-').\n"
                     "When stdout is not a terminal, rless behaves like rcat and writes\n"
                     "the rendered output without paging.\n"
                     "Inside the pager, press 'h' for keybindings or 'q' to quit."));

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
        .help(_("emit colors even on non-TTY (useful with --no-pager)"))
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
                "Markdown"))
        .metavar("LANG")
        .default_value(std::string());

    app.add_argument("--no-pager")
        .help(_("never page: write the rendered output and exit (like rcat)"))
        .default_value(false)
        .implicit_value(true);

    app.add_argument("files")
        .help(_("file(s); use '-' for stdin. Markdown by default, or any source/config file"))
        .metavar("FILE")
        .remaining();

    try {
        app.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "rless: %s\n%s\n", e.what(),
                     _("Try 'rless --help' for more information."));
        return 2;
    }

    if (app.get<bool>("--version")) {
        std::printf("rless %s\n", rcat::kVersion);
        return 0;
    }

    rcat::RenderOptions opts;
    opts.caps = probe_tty_caps();

    if (auto n = app.present<int>("--columns")) {
        if (*n < 10) {
            std::fprintf(stderr, "rless: %s\n", _("--columns must be >= 10"));
            return 2;
        }
        opts.caps.columns = *n;
    }
    if (auto n = app.present<int>("--image-height")) {
        if (*n < 1) {
            std::fprintf(stderr, "rless: %s\n", _("--image-height must be >= 1"));
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
            std::fprintf(stderr, "rless: %s\n", _("--web-timeout must be >= 1"));
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
        std::fprintf(stderr, "rless: %s\n", _("invalid --color value"));
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
    } catch (const std::logic_error&) {}
    if (files.empty())
        files.emplace_back("-");

    const std::string forced_lang = app.get<std::string>("--lang");
    const bool no_pager = app.get<bool>("--no-pager") || !isatty(STDOUT_FILENO);

    // Render every file up front (so we can page through them back-to-back).
    // Each file is separated by a single blank line.
    int exit_code = 0;
    std::string combined;
    std::vector<std::pair<fs::path, std::string>> raw_inputs;
    for (const auto& f : files) {
        const fs::path path{f};
        auto src_opt = read_file(path);
        if (!src_opt) {
            exit_code = 1;
            continue;
        }
        if (!combined.empty())
            combined += "\n";
        combined += render_one(*src_opt, path, forced_lang, opts, opts.caps.columns);
        raw_inputs.emplace_back(path, std::move(*src_opt));
    }

    if (no_pager || raw_inputs.empty()) {
        write_to_stdout(combined);
        return exit_code;
    }

    // The pager wants a rerender callback so resize works.
    auto rerender = [&, forced_lang](int cols) {
        std::string out;
        for (std::size_t i = 0; i < raw_inputs.size(); ++i) {
            if (i > 0)
                out += "\n";
            out += render_one(raw_inputs[i].second, raw_inputs[i].first, forced_lang, opts, cols);
        }
        return out;
    };

    rcat::PagerOptions popts;
    popts.no_color = opts.caps.color == rcat::ColorMode::None;
    if (raw_inputs.size() == 1 && raw_inputs.front().first != "-") {
        popts.filename = raw_inputs.front().first.string();
    } else if (raw_inputs.size() == 1) {
        popts.filename = "(stdin)";
    } else {
        popts.filename = std::to_string(raw_inputs.size()) + " files";
    }
    popts.rerender = rerender;

    int prc = rcat::run_pager(std::move(combined), popts);
    if (prc != 0) {
        // /dev/tty unavailable — fall back to dump.
        write_to_stdout(rerender(opts.caps.columns));
    }
    return exit_code;
}
