// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

#include "terminal.hpp"

#include <string>
#include <string_view>

namespace rcat {

// Broad token classes the renderer can color. Languages map their concrete
// constructs (a keyword, a JSON property name, an XML attribute, …) onto
// whichever class fits best — the colour palette lives in the renderer.
enum class TokenKind {
    Text,  // plain content, default colour
    Comment,
    Keyword,
    BuiltinType,  // int, float, str, bool, …
    Builtin,      // print, len, true, null, NaN, …
    Function,     // identifier immediately followed by '('
    Constant,     // UPPER_CASE identifier, language `true`/`false`
    Number,
    String,
    StringEscape,  // \n, \xff, ${…} inside a template string
    Operator,
    Punctuation,
    Tag,           // XML/HTML tag name
    Attribute,     // XML/HTML/JSX attribute name, INI key, YAML key
    Property,      // CSS property
    Section,       // INI/TOML [section] header
    Selector,      // CSS selector
    Decorator,     // @decorator (Python, Java), #[attribute] (Rust)
    Preprocessor,  // #include, %include, shebang line
    Variable,      // $var (shell), @var (Perl/Ruby), %var (Perl)
    Heading,       // markdown ##, restructured-text =====
    Emphasis,
    Url,
    Error,  // unterminated string / malformed escape
};

// Symbolic identifiers for every language we know how to tokenise. Adding a
// new language means: extend this enum, teach detect_language() about its
// extension/shebang, and add a tokenise_<name>() case in highlight.cpp.
enum class Language {
    PlainText,

    // C-family (curly braces, // and /* comments)
    C,
    Cpp,
    Java,
    JavaScript,
    TypeScript,
    CSharp,
    Go,
    Rust,
    Kotlin,
    Scala,
    Swift,
    Dart,
    ObjectiveC,
    Php,
    Groovy,
    Zig,
    V,
    Nim,
    Crystal,
    D,

    // Hash-comment scripts
    Python,
    Ruby,
    Perl,
    R,
    Julia,
    Shell,
    PowerShell,
    Tcl,

    // Dash-comment languages
    Sql,
    Lua,
    Haskell,
    Elm,
    Ada,

    // Semicolon-comment (Lisp family + asm)
    Lisp,
    Scheme,
    Clojure,
    Asm,

    // ML-style (* … *) comments
    OCaml,
    FSharp,

    // Erlang / Elixir
    Elixir,
    Erlang,

    // Markup
    Html,
    Xml,
    Svg,

    // Style
    Css,
    Scss,
    Less,

    // Structured config
    Json,
    Json5,
    Yaml,
    Toml,
    Ini,
    Env,
    Properties,

    // Build / ops
    Makefile,
    Dockerfile,
    CMake,
    Nginx,
    Apache,

    // Other / misc text-like
    Diff,
    Patch,
    GitConfig,
    GitIgnore,
    VimScript,
    Markdown,
    Latex,
};

// Detect a language from any of:
//   - a fenced-code-block info string ("```cpp" → "cpp"),
//   - a filename (".gitignore", "Dockerfile", "main.rs"),
//   - the first line of the file when it begins with "#!" (shebang).
// Any argument may be empty. Returns PlainText if nothing matches.
[[nodiscard]] Language detect_language(std::string_view info, std::string_view filename,
                                       std::string_view first_line = {});

// Map a TokenKind to an SGR style appropriate for a dark terminal. The
// palette is intentionally small and avoids backgrounds so it composes
// cleanly with the rest of the renderer's colour usage.
[[nodiscard]] Style style_for(TokenKind kind);

// Tokenise `source` and emit `source` with ANSI SGR sequences inserted
// between tokens. Each output line uses `prefix` as a leading string
// (e.g. block-indent + quote bars + 4-space code indent). Lines are
// emitted with a trailing '\n', including the last (caller can trim).
// Pass ColorMode::None to skip all escapes — the output then equals
// `prefix` + raw `source` with line breaks.
void highlight_to_stream(std::string_view source, Language lang, ColorMode color,
                         std::string_view prefix, std::string& out);

}  // namespace rcat
