// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "highlight.hpp"
#include "terminal.hpp"

#include <cstdio>
#include <string>
#include <string_view>

static int g_failed = 0;

#define CHECK(expr) do {                                                \
    if (!(expr)) {                                                      \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        ++g_failed;                                                     \
    }                                                                    \
} while (0)

static bool contains(const std::string& hay, std::string_view needle) {
    return hay.find(needle) != std::string::npos;
}

static std::string hl(std::string_view src, rcat::Language lang,
                      rcat::ColorMode mode = rcat::ColorMode::Ansi256) {
    std::string out;
    rcat::highlight_to_stream(src, lang, mode, /*prefix*/ "", out);
    return out;
}

int main() {
    using rcat::Language;
    using rcat::ColorMode;
    using rcat::detect_language;

    // ----------------- language detection -----------------
    CHECK(detect_language("cpp", "", "")              == Language::Cpp);
    CHECK(detect_language("c++", "", "")              == Language::Cpp);
    CHECK(detect_language("{.python}", "", "")        == Language::Python);
    CHECK(detect_language("", "foo.rs", "")           == Language::Rust);
    CHECK(detect_language("", "Makefile", "")         == Language::Makefile);
    CHECK(detect_language("", "Dockerfile", "")       == Language::Dockerfile);
    CHECK(detect_language("", "CMakeLists.txt", "")   == Language::CMake);
    CHECK(detect_language("", ".gitignore", "")       == Language::GitIgnore);
    CHECK(detect_language("", "config.toml", "")      == Language::Toml);
    CHECK(detect_language("", "config.yaml", "")      == Language::Yaml);
    CHECK(detect_language("", "config.yml", "")       == Language::Yaml);
    CHECK(detect_language("", "data.json", "")        == Language::Json);
    CHECK(detect_language("", "main.go", "")          == Language::Go);
    CHECK(detect_language("", "script.py", "")        == Language::Python);
    CHECK(detect_language("", "script", "#!/usr/bin/env python3\n") == Language::Python);
    CHECK(detect_language("", "script", "#!/bin/bash\n")             == Language::Shell);

    // ----------------- plain text -----------------
    {
        std::string out = hl("hello world\n", Language::PlainText, ColorMode::None);
        CHECK(out == "hello world\n");
    }

    // ----------------- highlighter emits SGR for keyword tokens -----------------
    {
        std::string out = hl("int x = 1;\n", Language::C);
        CHECK(contains(out, "\x1b["));    // any SGR
        CHECK(contains(out, "int"));
        CHECK(contains(out, "1"));
        CHECK(contains(out, ";"));
    }

    // ----------------- prefix is applied per line -----------------
    {
        std::string out;
        rcat::highlight_to_stream("a\nb\n", Language::PlainText,
                                  ColorMode::None, "> ", out);
        CHECK(out == "> a\n> b\n");
    }

    // ----------------- per-language smoke tests -----------------
    auto smoke = [&](Language lang, std::string_view src,
                     std::string_view must_contain) {
        std::string out = hl(src, lang);
        bool ok = contains(out, must_contain);
        if (!ok) {
            std::fprintf(stderr, "FAIL smoke lang=%d needle=%.*s out=%s\n",
                         (int)lang,
                         (int)must_contain.size(), must_contain.data(),
                         out.c_str());
            ++g_failed;
        }
    };

    // C family: keyword colored
    smoke(Language::C,          "int main() { return 0; }\n", "main");
    smoke(Language::Cpp,        "auto x = std::vector<int>{};\n", "vector");
    smoke(Language::Java,       "public class Foo {}\n", "public");
    smoke(Language::JavaScript, "const x = () => 42;\n", "const");
    smoke(Language::TypeScript, "type T = number | string;\n", "type");
    smoke(Language::CSharp,     "public void Foo() {}\n", "void");
    smoke(Language::Go,         "package main\nfunc main(){}\n", "func");
    smoke(Language::Rust,       "fn main() { println!(\"hi\"); }\n", "fn");
    smoke(Language::Kotlin,     "fun main() {}\n", "fun");
    smoke(Language::Swift,      "func main() {}\n", "func");
    smoke(Language::Dart,       "void main() {}\n", "main");
    smoke(Language::ObjectiveC, "@interface Foo : NSObject @end\n", "interface");
    smoke(Language::Php,        "<?php function f() {} ?>\n", "function");
    smoke(Language::Zig,        "pub fn main() void {}\n", "fn");

    // Scripts
    smoke(Language::Python,     "def f(x):\n    return x\n", "def");
    smoke(Language::Ruby,       "def foo; :sym; end\n", "def");
    smoke(Language::Perl,       "my $x = 42;\n", "$x");
    smoke(Language::R,          "x <- c(1, 2, 3)\n", "<-");
    smoke(Language::Julia,      "function f(x) end\n", "function");
    smoke(Language::Shell,      "echo $HOME\n", "$HOME");
    smoke(Language::PowerShell, "Get-ChildItem -Path .\n", "Get-ChildItem");
    smoke(Language::Tcl,        "puts \"hi\"\n", "puts");

    // Config formats
    smoke(Language::Json,       "{\"k\": 1}\n", "k");
    smoke(Language::Json5,      "{ k: 1, /* c */ }\n", "/*");
    smoke(Language::Yaml,       "k: v\n", "k");
    smoke(Language::Toml,       "[section]\nkey = 1\n", "section");
    smoke(Language::Ini,        "[s]\nk=v\n", "k");
    smoke(Language::Env,        "FOO=bar\n", "FOO");
    smoke(Language::Properties, "k=v\n", "k");

    // Markup / style
    smoke(Language::Html,       "<a href=\"#\">x</a>\n", "href");
    smoke(Language::Xml,        "<foo bar=\"1\"/>\n", "foo");
    smoke(Language::Css,        "a { color: red; }\n", "color");
    smoke(Language::Scss,       "$c: red; a { color: $c; }\n", "$c");

    // SQL
    smoke(Language::Sql,        "SELECT * FROM t WHERE a = 1;\n", "SELECT");

    // Lua
    smoke(Language::Lua,        "local x = 1\nprint(x)\n", "local");

    // Haskell / Elm / Ada
    smoke(Language::Haskell,    "main = putStrLn \"hi\"\n", "main");
    smoke(Language::Elm,        "main = text \"hi\"\n", "main");
    smoke(Language::Ada,        "procedure Foo is begin null; end Foo;\n", "begin");

    // Lisp family
    smoke(Language::Lisp,       "(defun f (x) (+ x 1))\n", "defun");
    smoke(Language::Scheme,     "(define (f x) (+ x 1))\n", "define");
    smoke(Language::Clojure,    "(defn f [x] (inc x))\n", "defn");
    smoke(Language::Asm,        "mov rax, 60\nsyscall\n", "mov");

    // ML family
    smoke(Language::OCaml,      "let f x = x + 1\n", "let");
    smoke(Language::FSharp,     "let f x = x + 1\n", "let");

    // BEAM
    smoke(Language::Erlang,     "main() -> ok.\n", "main");
    smoke(Language::Elixir,     "def f(x), do: x + 1\n", "def");

    // Build / ops
    smoke(Language::Makefile,
        "all: foo.o\n\tgcc -o all foo.o\n", "all");
    smoke(Language::Dockerfile, "FROM alpine\nRUN ls\n", "FROM");
    smoke(Language::CMake,      "project(foo)\nadd_executable(x x.c)\n", "project");
    smoke(Language::Nginx,      "server { listen 80; }\n", "server");

    // Misc
    smoke(Language::Diff,
        "diff --git a/x b/x\n--- a/x\n+++ b/x\n@@ -1 +1 @@\n-old\n+new\n",
        "old");
    smoke(Language::GitIgnore,  "*.o\n!main.o\n", "main.o");
    smoke(Language::VimScript,  "let g:x = 1\n", "let");
    smoke(Language::Markdown,   "# Heading\n* item\n", "Heading");
    smoke(Language::Latex,      "\\section{Hi} $x^2$\n", "section");

    if (g_failed == 0) std::printf("test_highlight: OK\n");
    return g_failed == 0 ? 0 : 1;
}
