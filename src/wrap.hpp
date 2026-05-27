// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace rcat {

// Decode one UTF-8 codepoint at position `i`. Returns bytes consumed (>=1).
int utf8_decode(std::string_view s, size_t i, uint32_t& cp);

// Cell width of a codepoint via wcwidth (with safe defaults).
[[nodiscard]] int char_width(uint32_t cp);

// Display width of a UTF-8 string in cells, skipping ANSI CSI/OSC escapes.
[[nodiscard]] int display_width(std::string_view s);

// Append SGR-aware word-wrapped output of `text` to `out`. Wraps to at most
// `columns` cells per line. First line is prefixed by `first_prefix`,
// continuation lines by `cont_prefix` (both are written verbatim and counted
// for width via display_width). A literal '\n' in `text` forces a break.
// If `style_open` is non-empty, every continuation line re-applies it after
// the prefix; this keeps a running style alive across wrapped lines.
void emit_wrapped(std::string& out,
                  std::string_view text,
                  std::string_view first_prefix,
                  std::string_view cont_prefix,
                  int columns,
                  std::string_view style_open = {});

} // namespace rcat
