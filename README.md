# rcat — render Markdown (and 60+ source/config formats) in the terminal

`rcat` is a small, fast C++17 CLI that renders Markdown files inline in your
terminal — with ANSI styling, OSC 8 hyperlinks, Unicode box-drawing,
syntax-highlighted code fences, and optional inline image rendering via
[chafa]. When the input isn't Markdown, `rcat` falls back to a built-in
syntax highlighter that knows ~60 languages and config formats, so it works
as a drop-in `cat` replacement that *also* pretty-prints whatever you throw
at it.

A companion binary, **`rless`**, takes the same rendered output and runs it
through an interactive `less`-style pager — scroll, page, jump to top/bottom,
and search through Markdown or source files the same way you would `less` a
log file.

```
rcat README.md
rcat src/rcat.cpp
curl -fsSL https://example.com/post.md | rcat -

rless README.md          # interactive pager
rless src/rcat.cpp       # same, with syntax highlighting
```

[chafa]: https://hpjansson.org/chafa/

## Features

- **CommonMark + GFM-ish Markdown** via [md4c]: headings, lists,
  blockquotes, tables, task lists, autolinks, strikethrough.
- **Syntax highlighting** for ~60 languages — C/C++, Rust, Go, Python,
  Ruby, Perl, Shell, PowerShell, SQL, Lua, Haskell, OCaml, Erlang,
  Lisps, JSON/YAML/TOML/INI, HTML/XML/CSS, Dockerfile, Makefile, CMake,
  diff/patch, and more. Run `rcat --lang list` for the full set.
- **Inline images** (PNG/JPG/GIF/BMP/PSD/TGA/PNM) rendered as Unicode
  half-block art via chafa; works over SSH, tmux, and any colour terminal.
- **Remote images** with explicit opt-in (`--allow-web`, requires
  libcurl). Off by default; size- and time-capped.
- **Terminal-aware**: autodetects truecolour / 256 / 16 / mono, OSC 8
  hyperlinks, UTF-8 vs ASCII, terminal width, and `NO_COLOR`.
- **Internationalised** via gettext (zh\_CN, es, ja, de bundled).
- **No GC, no runtime, one binary** — ~200 KB stripped, links only what
  it needs.

[md4c]: https://github.com/mity/md4c

## Install

### From source

Build requirements:

| Component | Notes |
| --- | --- |
| C++17 compiler | gcc ≥ 8, clang ≥ 7, AppleClang ≥ 11 |
| CMake | ≥ 3.14 |
| [md4c] | required |
| [argparse] | required (header-only, find via CMake) |
| libintl / gettext | optional, enables translated messages |
| [chafa] + [stb\_image.h] | optional, enables inline image rendering |
| libcurl | optional, enables `--allow-web` |

[argparse]: https://github.com/p-ranav/argparse
[stb\_image.h]: https://github.com/nothings/stb

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build           # installs to /usr/local by default
```

To disable optional features at configure time:

```sh
cmake -S . -B build \
    -DRCAT_ENABLE_IMAGES=OFF \
    -DRCAT_BUILD_TESTS=OFF
```

### Distribution packages

`cmake --build build --target package` produces `.tar.gz`, `.deb`, and
`.rpm` archives via CPack when the host has the matching tooling.

## Usage

```
rcat [OPTIONS] [FILE...]
```

Reads from stdin if no `FILE` is given (or `FILE` is `-`). Image paths in
Markdown are resolved relative to each input file's directory.

Common flags (`rcat --help` for the full list, in your locale):

| Flag | Effect |
| --- | --- |
| `-c, --columns N` | wrap to N columns (default: terminal width) |
| `-p, --plain`     | strip styling entirely (just wrapped text) |
| `--no-color`      | disable colour (keep Unicode/structure) |
| `--force-color`   | emit colours even when stdout isn't a TTY |
| `--ascii`         | ASCII-only output (no box-drawing/bullets) |
| `--color MODE`    | `auto`, `none`, `16`, `256`, `truecolor` |
| `--no-images`     | skip inline images even when chafa is built in |
| `--allow-web`     | download `http(s)` image URLs (off by default) |
| `--web-timeout S` | per-image fetch timeout (default 10s) |
| `--image-height N`| max rendered image height in rows (default 20) |
| `--no-hyperlinks` | disable OSC 8 even when terminal supports it |
| `--lang LANG`     | force language; `md` for Markdown, `list` to print all |
| `-V, --version`   | print version |

### Environment

| Variable | Effect |
| --- | --- |
| `NO_COLOR`        | disable colour entirely (any value) |
| `COLUMNS`         | override terminal-width autodetect |
| `LANG`, `LC_*`    | drive locale + UTF-8 detection |
| `RCAT_LOCALEDIR`  | where to look for `.mo` catalogs (defaults to install prefix) |

### Exit codes

| Code | Meaning |
| --- | --- |
| `0` | success |
| `1` | one or more inputs failed to read or parse |
| `2` | bad command-line usage |

## Examples

Pipe through `less` while keeping colour:

```sh
rcat --force-color README.md | less -R
```

Pretty-print any source file `cat`-style:

```sh
rcat src/rcat.cpp
rcat /etc/hosts
rcat Dockerfile
```

Force a specific language for a file with an unusual extension:

```sh
rcat --lang yaml config.txt
```

## `rless` — the pager

`rless` shares everything above (renderer, highlighter, terminal-cap
detection, i18n) and adds an interactive viewport with `less`-style key
bindings:

| Key                  | Action                          |
| ---                  | ---                             |
| `j`, Down            | down one line                   |
| `k`, Up              | up one line                     |
| `Space`, `f`, PageDn | down one page                   |
| `b`, PageUp          | up one page                     |
| `d` / `u`            | down / up half a page           |
| `g`, Home            | top of buffer                   |
| `G`, End             | bottom of buffer                |
| `/pattern`           | search forward (ASCII-icase)    |
| `?pattern`           | search backward                 |
| `n` / `N`            | next / previous match           |
| `h`, `H`             | in-pager help                   |
| `q`, `Q`, Ctrl-C     | quit                            |

When stdout isn't a terminal — `rless foo.md | grep bar`, redirection to a
file, etc. — or when `--no-pager` is given, `rless` skips the viewport and
writes the rendered output, so it composes with pipelines like `rcat` does.
Terminal resizes are handled by re-wrapping the buffer to the new width.

```sh
rless README.md                       # interactive
rless --no-pager src/rcat.cpp         # behave like rcat
rless --force-color README.md | aha   # ANSI → HTML
```

`rless` accepts the same renderer flags as `rcat` (`--color`, `--columns`,
`--no-images`, `--allow-web`, `--ascii`, `--lang`, …). See `rless --help`
or `man rless` for the full list.

## Development

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional sanitizer builds:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DRCAT_ENABLE_ASAN=ON
cmake --build build-asan -j && ctest --test-dir build-asan
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the contribution flow, code
style, and translation instructions.

## License

Released under the [GNU General Public License v3.0 or later](LICENSE).
`SPDX-License-Identifier: GPL-3.0-or-later`.
