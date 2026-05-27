# Contributing to rcat

Thanks for taking the time to contribute! This document is short on
purpose — read it once, then send your patch.

## Ground rules

- Be kind. We follow the [Contributor Covenant](CODE_OF_CONDUCT.md).
- Security issues go through the process in [SECURITY.md](SECURITY.md),
  not the public issue tracker.
- One change per PR. Bundle a refactor and a behaviour change only when
  the refactor is necessary to make the behaviour change reviewable.

## Building locally

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional toggles:

| Flag | Effect |
| --- | --- |
| `-DRCAT_ENABLE_IMAGES=OFF` | drop the chafa/libcurl dependency at build time |
| `-DRCAT_BUILD_TESTS=OFF`   | skip the test targets |
| `-DRCAT_ENABLE_ASAN=ON`    | AddressSanitizer + UBSan |
| `-DRCAT_ENABLE_TSAN=ON`    | ThreadSanitizer (mutually exclusive with ASan) |
| `-DRCAT_ENABLE_COVERAGE=ON`| gcov instrumentation |

CI runs the same configure + build + ctest across Linux/macOS and
gcc/clang. Please make sure your change passes locally before pushing.

## Style

- C++17 (the codebase keeps the standard low so it builds on every
  distro's default compiler). Use modern facilities — `std::string_view`,
  `std::optional`, `std::filesystem`, structured bindings, RAII — but
  do not raise `CMAKE_CXX_STANDARD` without prior discussion.
- Format with `clang-format` (config in `.clang-format`). The CI
  `format` job will fail on a dirty diff.
- Lint with `clang-tidy` (config in `.clang-tidy`). The CI `tidy` job
  is advisory but please fix what you can.
- 4-space indent, no tabs. ~100-column soft limit.
- Comments explain *why*, not *what*. Function/file/method names should
  carry the *what*.
- All user-facing strings go through the `_("...")` gettext macro from
  `i18n.hpp` so translations can pick them up.

## Adding a language to the highlighter

1. Add a new entry to `enum class Language` in `src/highlight.hpp`.
2. Teach `detect_language()` in `src/highlight.cpp` about the
   extension, filename, or shebang.
3. Add a `tokenise_<name>()` and dispatch from `highlight_to_stream()`.
   Put the implementation in a new `src/lang/lang_<name>.cpp` and
   register it in `add_library(rcat_core …)`.
4. Add a sample fixture and a CLI test case in `tests/`.

## Translations

1. Regenerate the `.pot` template from sources:
   ```sh
   xgettext --keyword=_ --keyword=N_ \
       --from-code=UTF-8 -o po/rcat.pot src/*.cpp src/*.hpp src/lang/*.cpp
   ```
2. Merge new strings into existing catalogs:
   ```sh
   for po in po/*.po; do msgmerge --update --backup=none "$po" po/rcat.pot; done
   ```
3. Translate (or fix translations) with your editor of choice. Run
   `msgfmt --check po/<lang>.po` before committing.

## Commit messages

Imperative mood, ~72 char subject, blank line, then prose if the change
needs explaining beyond the diff. Reference issues with `Fixes #123`
when applicable. Sign-off (`Signed-off-by:`) is not required.

## Reporting bugs

Open an issue with:

- `rcat --version`
- Your terminal, OS, and `echo "$TERM $COLORTERM $LANG"`
- A minimal input that reproduces the problem
- What you expected vs what you got (pasted output, or a screenshot
  for visual glitches)

Thanks again!
