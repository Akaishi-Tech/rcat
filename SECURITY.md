# Security Policy

## Supported versions

`rcat` is pre-1.0 and only the `main` branch receives security updates.
Once the project tags a `1.x` release, the most recent minor version
will be supported for security fixes.

| Version | Supported |
| --- | --- |
| `main` | ✅ |
| anything else | ❌ |

## Reporting a vulnerability

**Do not open a public GitHub issue for security reports.** Instead,
email the maintainers privately:

> hcgstd@proton.me

Please include:

- a description of the issue and its impact,
- a minimal reproducer (input file, command line, environment),
- the version of `rcat` you tested (`rcat --version`),
- and, if you have one, a suggested fix.

We aim to:

1. Acknowledge your report within **3 business days**.
2. Confirm or dispute the issue within **10 business days**.
3. Ship a fix and a coordinated public disclosure within **90 days** of
   the initial report, unless we agree on a longer window with you.

## Scope

`rcat` reads untrusted Markdown and source files. The threat model we
explicitly defend against:

- Malformed UTF-8, oversized inputs, malformed Markdown, malformed code
  fences, malformed image bytes.
- Adversarial filenames or info-strings.
- Adversarial *remote* image URLs when `--allow-web` is passed (size
  cap, timeout, content-type sniffing happens via stb\_image — no shell
  execution).

The threat model we do **not** defend against:

- A malicious `MD4C`, `chafa`, `libcurl`, or `stb_image.h` build —
  these are trusted dependencies.
- Side-channel attacks on your terminal emulator (e.g. shell-execution
  bugs in the terminal triggered by exotic escape sequences). `rcat`
  emits a small, well-known subset of ANSI/OSC, but a buggy terminal is
  out of scope.

Thank you for helping keep `rcat` and its users safe.
