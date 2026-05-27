<!-- Thanks for the contribution! Please read CONTRIBUTING.md before opening. -->

## Summary

<!-- 1–3 sentences: what does this change and why? -->

## Type of change

- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Breaking change (CLI flags, exit codes, output, or public API)
- [ ] Refactor / cleanup (no behaviour change)
- [ ] Documentation / build / CI only

## Checklist

- [ ] `ctest --test-dir build --output-on-failure` passes locally
- [ ] `clang-format -i` was run on touched files
- [ ] User-visible changes are in `CHANGELOG.md` under `## [Unreleased]`
- [ ] User-facing strings are wrapped in `_("...")` so gettext picks them up
- [ ] Adding a language? `tests/fixtures/` has a sample and a CLI test asserts colour output

## Related issues

<!-- "Fixes #123", "Closes #456", or "Refs #789" -->
