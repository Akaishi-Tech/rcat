// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rcat {

// ---------------------------------------------------------------------------
// Headless helpers — separated from the interactive loop so they are unit
// testable. All take UTF-8 input that may carry ANSI CSI/OSC escapes.
// ---------------------------------------------------------------------------

// Split `rendered` into visible lines on '\n'. Returned views point into the
// input; the caller must keep `rendered` alive. The terminating newline
// is excluded from each view, and any trailing empty line after a final '\n'
// is dropped (matches the behaviour `less` shows for a file ending in '\n').
[[nodiscard]] std::vector<std::string_view>
split_visible_lines(std::string_view rendered);

// Return `s` with all ANSI CSI/OSC escape sequences removed.
[[nodiscard]] std::string strip_ansi(std::string_view s);

// First line index at-or-after `from` whose ANSI-stripped text contains
// `needle`. `case_sensitive == false` does a simple ASCII case fold.
[[nodiscard]] std::optional<std::size_t>
find_next_match(const std::vector<std::string_view>& lines,
                std::string_view needle,
                std::size_t from,
                bool case_sensitive);

// First line index at-or-before `from` matching `needle`. Mirrors the above.
[[nodiscard]] std::optional<std::size_t>
find_prev_match(const std::vector<std::string_view>& lines,
                std::string_view needle,
                std::size_t from,
                bool case_sensitive);

// Clamp `top` so that `[top, top + viewport)` stays inside `[0, total)`.
// Returns 0 when `total == 0`. Always returns a value <= max(0, total-1).
[[nodiscard]] std::size_t
clamp_top(std::ptrdiff_t top, std::size_t viewport, std::size_t total);

// ---------------------------------------------------------------------------
// Interactive pager
// ---------------------------------------------------------------------------

struct PagerOptions {
    // Shown in the status bar. Empty → "(stdin)".
    std::string filename;

    // When false, the pager emits a reverse-video SGR for the status line.
    // When true, the status line is plain text (chosen by callers that
    // already disabled color globally).
    bool no_color = false;

    // Optional: invoked with a new column count when the terminal is resized
    // so the caller can re-wrap. The returned string replaces the current
    // rendered buffer. May be empty (no callback) — in that case resize is
    // handled by re-painting the existing buffer at the new size.
    std::function<std::string(int columns)> rerender;
};

// Run the pager on `rendered`. Reads keys from /dev/tty and paints to
// /dev/tty. Returns 0 on clean exit, non-zero if /dev/tty cannot be opened
// or termios setup fails (in that case the caller should fall back to a
// non-paged dump).
[[nodiscard]] int run_pager(std::string rendered, const PagerOptions& opts);

} // namespace rcat
