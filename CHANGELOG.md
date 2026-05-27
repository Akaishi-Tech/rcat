# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-05-27

### Added
- Initial public source release of `rcat`: Markdown renderer for the
  terminal with ANSI/Unicode styling, OSC 8 hyperlinks, inline images
  via chafa, remote-image fetch via libcurl, and a built-in syntax
  highlighter for ~60 languages and config formats.
- `rless`: companion paging UI built on the same renderer and
  highlighter as `rcat`. `less`-style key bindings
  (`j`/`k`/Space/`b`/`g`/`G`/`/`/`n`), ASCII case-insensitive search
  across ANSI-styled lines, automatic re-wrap on terminal resize, and
  a dump-mode fallback when stdout is not a TTY (so
  `rless foo.md | grep bar` still composes). Ships with its own
  `rless.1` man page; shares the `rcat.mo` translation catalog.
- gettext-backed i18n with translations for zh\_CN, es, ja, de.
- CMake project with `RCAT_BUILD_TESTS`, `RCAT_INSTALL_MAN`, and
  sanitizer/coverage toggles.
- CPack packaging targets (tar.gz, DEB, RPM).
- `rcat.1` and `rless.1` man pages.
- ctest suite covering wrap, render, highlight, image, pager, CLI
  flags, and the zh\_CN translation path.
- Project licensed under GPL-3.0-or-later.

### Build
- All external dependencies — md4c, argparse, chafa, stb\_image,
  libcurl, and libintl/gettext — are required at configure time.
  Conditional `RCAT_HAVE_*` paths in the C++ sources have been
  removed; the build either has these libraries or fails at configure.
- CI matrix is `ubuntu-24.04` × `{gcc, clang}` × `{Debug, Release}`,
  plus ASan, TSan, clang-format, and clang-tidy jobs.

[Unreleased]: https://github.com/Akaishi-Tech/rcat/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Akaishi-Tech/rcat/releases/tag/v1.0.0
