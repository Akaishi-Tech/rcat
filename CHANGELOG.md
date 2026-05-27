# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `rless`: companion paging UI built on the same renderer and
  highlighter as `rcat`. `less`-style key bindings
  (`j`/`k`/Space/`b`/`g`/`G`/`/`/`n`), ASCII case-insensitive search
  across ANSI-styled lines, automatic re-wrap on terminal resize, and
  a dump-mode fallback when stdout is not a TTY (so
  `rless foo.md | grep bar` still composes). Ships with its own
  `rless.1` man page; shares the `rcat.mo` translation catalog.
- Project licensed under GPL-3.0-or-later.
- Initial public source release of `rcat`: Markdown renderer for the
  terminal with ANSI/Unicode styling, OSC 8 hyperlinks, inline images
  via chafa, optional remote-image fetch via libcurl, and a built-in
  syntax highlighter for ~60 languages and config formats.
- gettext-backed i18n with translations for zh\_CN, es, ja, de.
- CMake project with optional `RCAT_ENABLE_IMAGES`, `RCAT_BUILD_TESTS`,
  and sanitizer/coverage toggles.
- CPack packaging targets (tar.gz, DEB, RPM).
- `rcat.1` man page.
- ctest suite covering wrap, render, highlight, image, CLI flags, and
  the zh\_CN translation path.

[Unreleased]: https://github.com/Akaishi-Tech/rcat/compare/HEAD...HEAD
