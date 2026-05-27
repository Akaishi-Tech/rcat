// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "highlight.hpp"

#include "lang/lang_common.hpp"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

// Forward declarations of every tokeniser we ship. Each lives in its own
// translation unit; removing the .cpp file (and its register_* line below)
// drops support cleanly. Adding a language: add to highlight.hpp's Language
// enum, declare its tokeniser here, implement it in a new src/lang/lang_*.cpp,
// and add a register_tokeniser() call in init_registry() below.
namespace rcat::lang {

#define DECL(name) void tokenise_##name(std::string_view, const TokenSink&)

DECL(c);
DECL(cpp);
DECL(java);
DECL(javascript);
DECL(typescript);
DECL(csharp);
DECL(go);
DECL(rust);
DECL(kotlin);
DECL(scala);
DECL(swift);
DECL(dart);
DECL(objectivec);
DECL(php);
DECL(groovy);
DECL(zig);
DECL(v);
DECL(nim);
DECL(crystal);
DECL(d);

DECL(python);
DECL(ruby);
DECL(perl);
DECL(r);
DECL(julia);
DECL(shell);
DECL(powershell);
DECL(tcl);

DECL(sql);
DECL(lua);
DECL(haskell);
DECL(elm);
DECL(ada);

DECL(lisp);
DECL(scheme);
DECL(clojure);
DECL(asm);

DECL(ocaml);
DECL(fsharp);
DECL(elixir);
DECL(erlang);

DECL(html);
DECL(xml);
DECL(svg);
DECL(css);
DECL(scss);
DECL(less);

DECL(json);
DECL(json5);
DECL(yaml);
DECL(toml);
DECL(ini);
DECL(env);
DECL(properties);

DECL(makefile);
DECL(dockerfile);
DECL(cmake);
DECL(nginx);
DECL(apache);

DECL(diff);
DECL(patch);
DECL(gitconfig);
DECL(gitignore);
DECL(vimscript);
DECL(markdown);
DECL(latex);

#undef DECL

}  // namespace rcat::lang

namespace rcat {

namespace {

using lang::TokeniseFn;

struct Registry {
    std::unordered_map<int, TokeniseFn> map;
};

Registry& registry() {
    static Registry r;
    return r;
}

void reg(Language l, TokeniseFn fn) {
    registry().map.emplace(static_cast<int>(l), fn);
}

void init_registry() {
    static bool done = false;
    if (done)
        return;
    done = true;
    using namespace lang;

    reg(Language::C, tokenise_c);
    reg(Language::Cpp, tokenise_cpp);
    reg(Language::Java, tokenise_java);
    reg(Language::JavaScript, tokenise_javascript);
    reg(Language::TypeScript, tokenise_typescript);
    reg(Language::CSharp, tokenise_csharp);
    reg(Language::Go, tokenise_go);
    reg(Language::Rust, tokenise_rust);
    reg(Language::Kotlin, tokenise_kotlin);
    reg(Language::Scala, tokenise_scala);
    reg(Language::Swift, tokenise_swift);
    reg(Language::Dart, tokenise_dart);
    reg(Language::ObjectiveC, tokenise_objectivec);
    reg(Language::Php, tokenise_php);
    reg(Language::Groovy, tokenise_groovy);
    reg(Language::Zig, tokenise_zig);
    reg(Language::V, tokenise_v);
    reg(Language::Nim, tokenise_nim);
    reg(Language::Crystal, tokenise_crystal);
    reg(Language::D, tokenise_d);

    reg(Language::Python, tokenise_python);
    reg(Language::Ruby, tokenise_ruby);
    reg(Language::Perl, tokenise_perl);
    reg(Language::R, tokenise_r);
    reg(Language::Julia, tokenise_julia);
    reg(Language::Shell, tokenise_shell);
    reg(Language::PowerShell, tokenise_powershell);
    reg(Language::Tcl, tokenise_tcl);

    reg(Language::Sql, tokenise_sql);
    reg(Language::Lua, tokenise_lua);
    reg(Language::Haskell, tokenise_haskell);
    reg(Language::Elm, tokenise_elm);
    reg(Language::Ada, tokenise_ada);

    reg(Language::Lisp, tokenise_lisp);
    reg(Language::Scheme, tokenise_scheme);
    reg(Language::Clojure, tokenise_clojure);
    reg(Language::Asm, tokenise_asm);

    reg(Language::OCaml, tokenise_ocaml);
    reg(Language::FSharp, tokenise_fsharp);
    reg(Language::Elixir, tokenise_elixir);
    reg(Language::Erlang, tokenise_erlang);

    reg(Language::Html, tokenise_html);
    reg(Language::Xml, tokenise_xml);
    reg(Language::Svg, tokenise_svg);
    reg(Language::Css, tokenise_css);
    reg(Language::Scss, tokenise_scss);
    reg(Language::Less, tokenise_less);

    reg(Language::Json, tokenise_json);
    reg(Language::Json5, tokenise_json5);
    reg(Language::Yaml, tokenise_yaml);
    reg(Language::Toml, tokenise_toml);
    reg(Language::Ini, tokenise_ini);
    reg(Language::Env, tokenise_env);
    reg(Language::Properties, tokenise_properties);

    reg(Language::Makefile, tokenise_makefile);
    reg(Language::Dockerfile, tokenise_dockerfile);
    reg(Language::CMake, tokenise_cmake);
    reg(Language::Nginx, tokenise_nginx);
    reg(Language::Apache, tokenise_apache);

    reg(Language::Diff, tokenise_diff);
    reg(Language::Patch, tokenise_patch);
    reg(Language::GitConfig, tokenise_gitconfig);
    reg(Language::GitIgnore, tokenise_gitignore);
    reg(Language::VimScript, tokenise_vimscript);
    reg(Language::Markdown, tokenise_markdown);
    reg(Language::Latex, tokenise_latex);
}

std::string ascii_lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = (char)std::tolower((unsigned char)c);
    return out;
}

std::string_view basename(std::string_view path) {
    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

std::string_view extension(std::string_view name) {
    // Special-case dotfiles: ".bashrc" extension is "bashrc" (treated as
    // the name itself by the alias table below — return empty here).
    if (!name.empty() && name.front() == '.')
        return {};
    auto pos = name.find_last_of('.');
    if (pos == std::string_view::npos || pos + 1 == name.size())
        return {};
    return name.substr(pos + 1);
}

struct Alias {
    const char* key;
    Language lang;
};

// Map of info-string / extension aliases to Language. Order matters only for
// readability — lookup is linear.
const Alias kInfoMap[] = {
    {"c", Language::C},
    {"h", Language::C},
    {"cpp", Language::Cpp},
    {"c++", Language::Cpp},
    {"cxx", Language::Cpp},
    {"cc", Language::Cpp},
    {"hpp", Language::Cpp},
    {"hxx", Language::Cpp},
    {"hh", Language::Cpp},
    {"java", Language::Java},
    {"js", Language::JavaScript},
    {"javascript", Language::JavaScript},
    {"mjs", Language::JavaScript},
    {"cjs", Language::JavaScript},
    {"jsx", Language::JavaScript},
    {"ts", Language::TypeScript},
    {"typescript", Language::TypeScript},
    {"tsx", Language::TypeScript},
    {"cs", Language::CSharp},
    {"csharp", Language::CSharp},
    {"go", Language::Go},
    {"golang", Language::Go},
    {"rs", Language::Rust},
    {"rust", Language::Rust},
    {"kt", Language::Kotlin},
    {"kotlin", Language::Kotlin},
    {"kts", Language::Kotlin},
    {"scala", Language::Scala},
    {"sc", Language::Scala},
    {"swift", Language::Swift},
    {"dart", Language::Dart},
    {"m", Language::ObjectiveC},
    {"mm", Language::ObjectiveC},
    {"objc", Language::ObjectiveC},
    {"objective-c", Language::ObjectiveC},
    {"php", Language::Php},
    {"groovy", Language::Groovy},
    {"gradle", Language::Groovy},
    {"zig", Language::Zig},
    {"v", Language::V},
    {"nim", Language::Nim},
    {"nims", Language::Nim},
    {"cr", Language::Crystal},
    {"crystal", Language::Crystal},
    {"d", Language::D},

    {"py", Language::Python},
    {"python", Language::Python},
    {"pyw", Language::Python},
    {"rb", Language::Ruby},
    {"ruby", Language::Ruby},
    {"pl", Language::Perl},
    {"pm", Language::Perl},
    {"perl", Language::Perl},
    {"r", Language::R},
    {"jl", Language::Julia},
    {"julia", Language::Julia},
    {"sh", Language::Shell},
    {"bash", Language::Shell},
    {"zsh", Language::Shell},
    {"ksh", Language::Shell},
    {"shell", Language::Shell},
    {"ps1", Language::PowerShell},
    {"psm1", Language::PowerShell},
    {"powershell", Language::PowerShell},
    {"tcl", Language::Tcl},

    {"sql", Language::Sql},
    {"lua", Language::Lua},
    {"hs", Language::Haskell},
    {"haskell", Language::Haskell},
    {"elm", Language::Elm},
    {"ada", Language::Ada},
    {"adb", Language::Ada},
    {"ads", Language::Ada},

    {"lisp", Language::Lisp},
    {"lsp", Language::Lisp},
    {"cl", Language::Lisp},
    {"el", Language::Lisp},
    {"elisp", Language::Lisp},
    {"scm", Language::Scheme},
    {"ss", Language::Scheme},
    {"scheme", Language::Scheme},
    {"clj", Language::Clojure},
    {"cljs", Language::Clojure},
    {"cljc", Language::Clojure},
    {"clojure", Language::Clojure},
    {"asm", Language::Asm},
    {"s", Language::Asm},
    {"nasm", Language::Asm},
    {"gas", Language::Asm},

    {"ml", Language::OCaml},
    {"mli", Language::OCaml},
    {"ocaml", Language::OCaml},
    {"fs", Language::FSharp},
    {"fsi", Language::FSharp},
    {"fsx", Language::FSharp},
    {"fsharp", Language::FSharp},
    {"ex", Language::Elixir},
    {"exs", Language::Elixir},
    {"elixir", Language::Elixir},
    {"erl", Language::Erlang},
    {"hrl", Language::Erlang},
    {"erlang", Language::Erlang},

    {"html", Language::Html},
    {"htm", Language::Html},
    {"xhtml", Language::Html},
    {"xml", Language::Xml},
    {"svg", Language::Svg},
    {"plist", Language::Xml},
    {"rss", Language::Xml},
    {"atom", Language::Xml},

    {"css", Language::Css},
    {"scss", Language::Scss},
    {"sass", Language::Scss},
    {"less", Language::Less},

    {"json", Language::Json},
    {"json5", Language::Json5},
    {"jsonc", Language::Json5},
    {"yaml", Language::Yaml},
    {"yml", Language::Yaml},
    {"toml", Language::Toml},
    {"ini", Language::Ini},
    {"cfg", Language::Ini},
    {"conf", Language::Ini},
    {"env", Language::Env},
    {"dotenv", Language::Env},
    {"properties", Language::Properties},

    {"make", Language::Makefile},
    {"makefile", Language::Makefile},
    {"mk", Language::Makefile},
    {"dockerfile", Language::Dockerfile},
    {"cmake", Language::CMake},

    {"diff", Language::Diff},
    {"patch", Language::Patch},
    {"gitconfig", Language::GitConfig},
    {"gitignore", Language::GitIgnore},
    {"vim", Language::VimScript},
    {"vimscript", Language::VimScript},
    {"md", Language::Markdown},
    {"markdown", Language::Markdown},
    {"tex", Language::Latex},
    {"latex", Language::Latex},
};

// Filenames (case-insensitive) → Language, for files without a useful
// extension. Matched after the extension table.
const Alias kFilenameMap[] = {
    {"makefile", Language::Makefile},
    {"gnumakefile", Language::Makefile},
    {"bsdmakefile", Language::Makefile},
    {"dockerfile", Language::Dockerfile},
    {"containerfile", Language::Dockerfile},
    {"cmakelists.txt", Language::CMake},
    {".gitignore", Language::GitIgnore},
    {".gitconfig", Language::GitConfig},
    {".dockerignore", Language::GitIgnore},
    {".env", Language::Env},
    {".bashrc", Language::Shell},
    {".zshrc", Language::Shell},
    {".profile", Language::Shell},
    {".bash_profile", Language::Shell},
    {".vimrc", Language::VimScript},
    {"nginx.conf", Language::Nginx},
    {".htaccess", Language::Apache},
};

Language lookup(std::string_view key, const Alias* table, size_t count) {
    if (key.empty())
        return Language::PlainText;
    std::string k = ascii_lower(key);
    for (size_t i = 0; i < count; ++i) {
        if (k == table[i].key)
            return table[i].lang;
    }
    return Language::PlainText;
}

Language from_shebang(std::string_view line) {
    // "#! /usr/bin/env python3" → "python"; "#!/bin/bash" → "bash"; etc.
    if (line.size() < 2 || line[0] != '#' || line[1] != '!') {
        return Language::PlainText;
    }
    // Trim any trailing whitespace (newline, spaces) from the shebang line.
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' ||
                             line.back() == '\t')) {
        line.remove_suffix(1);
    }
    auto bn = basename(line);
    // "env <interp>" → <interp>
    auto sp = bn.find(' ');
    if (sp != std::string_view::npos) {
        std::string_view head = bn.substr(0, sp);
        std::string_view rest = bn.substr(sp + 1);
        // Skip any flags between env and the interpreter (e.g. "-S").
        while (!rest.empty() && rest.front() == ' ')
            rest.remove_prefix(1);
        if (head == "env" && !rest.empty()) {
            auto sp2 = rest.find(' ');
            bn = sp2 == std::string_view::npos ? rest : rest.substr(0, sp2);
        } else {
            bn = head;
        }
    }
    auto tab = bn.find('\t');
    if (tab != std::string_view::npos)
        bn = bn.substr(0, tab);
    // Strip a trailing version suffix ("python3", "ruby2.7") so the alias
    // table doesn't need to enumerate every version.
    while (!bn.empty() && (lang::is_digit(bn.back()) || bn.back() == '.')) {
        bn.remove_suffix(1);
    }
    if (bn.empty() || bn == "env")
        return Language::PlainText;
    return lookup(bn, kInfoMap, sizeof(kInfoMap) / sizeof(*kInfoMap));
}

}  // namespace

Language detect_language(std::string_view info, std::string_view filename,
                         std::string_view first_line) {
    if (!info.empty()) {
        // Pandoc-style ".cpp" or "{.cpp}" → "cpp"
        std::string_view s = info;
        while (!s.empty() && (s.front() == '{' || s.front() == ' '))
            s.remove_prefix(1);
        while (!s.empty() && (s.back() == '}' || s.back() == ' '))
            s.remove_suffix(1);
        if (!s.empty() && s.front() == '.')
            s.remove_prefix(1);
        auto sp = s.find_first_of(" \t,");
        if (sp != std::string_view::npos)
            s = s.substr(0, sp);
        Language l = lookup(s, kInfoMap, sizeof(kInfoMap) / sizeof(*kInfoMap));
        if (l != Language::PlainText)
            return l;
    }
    if (!filename.empty()) {
        auto bn = basename(filename);
        Language l = lookup(bn, kFilenameMap, sizeof(kFilenameMap) / sizeof(*kFilenameMap));
        if (l != Language::PlainText)
            return l;
        auto ext = extension(bn);
        if (!ext.empty()) {
            l = lookup(ext, kInfoMap, sizeof(kInfoMap) / sizeof(*kInfoMap));
            if (l != Language::PlainText)
                return l;
        }
    }
    if (!first_line.empty()) {
        Language l = from_shebang(first_line);
        if (l != Language::PlainText)
            return l;
    }
    return Language::PlainText;
}

Style style_for(TokenKind kind) {
    Style s;
    switch (kind) {
    case TokenKind::Text:
        break;
    case TokenKind::Comment:
        s.fg_256 = 245;
        s.italic = true;
        break;
    case TokenKind::Keyword:
        s.fg_256 = 135;
        s.bold = true;
        break;
    case TokenKind::BuiltinType:
        s.fg_256 = 81;
        break;
    case TokenKind::Builtin:
        s.fg_256 = 173;
        break;
    case TokenKind::Function:
        s.fg_256 = 220;
        break;
    case TokenKind::Constant:
        s.fg_256 = 209;
        break;
    case TokenKind::Number:
        s.fg_256 = 141;
        break;
    case TokenKind::String:
        s.fg_256 = 150;
        break;
    case TokenKind::StringEscape:
        s.fg_256 = 215;
        s.bold = true;
        break;
    case TokenKind::Operator:
        s.fg_256 = 251;
        break;
    case TokenKind::Punctuation:
        s.fg_256 = 247;
        break;
    case TokenKind::Tag:
        s.fg_256 = 75;
        s.bold = true;
        break;
    case TokenKind::Attribute:
        s.fg_256 = 117;
        break;
    case TokenKind::Property:
        s.fg_256 = 117;
        break;
    case TokenKind::Section:
        s.fg_256 = 39;
        s.bold = true;
        break;
    case TokenKind::Selector:
        s.fg_256 = 220;
        s.bold = true;
        break;
    case TokenKind::Decorator:
        s.fg_256 = 220;
        s.italic = true;
        break;
    case TokenKind::Preprocessor:
        s.fg_256 = 207;
        break;
    case TokenKind::Variable:
        s.fg_256 = 215;
        break;
    case TokenKind::Heading:
        s.fg_256 = 39;
        s.bold = true;
        break;
    case TokenKind::Emphasis:
        s.italic = true;
        break;
    case TokenKind::Url:
        s.fg_256 = 33;
        s.underline = true;
        break;
    case TokenKind::Error:
        s.fg_256 = 196;
        s.bold = true;
        break;
    }
    return s;
}

namespace lang {

TokeniseFn get_tokeniser(Language lang) {
    init_registry();
    auto it = registry().map.find(static_cast<int>(lang));
    return it == registry().map.end() ? nullptr : it->second;
}

}  // namespace lang

void highlight_to_stream(std::string_view source, Language lang, ColorMode color,
                         std::string_view prefix, std::string& out) {
    init_registry();

    auto fn = lang::get_tokeniser(lang);
    const bool styled = color != ColorMode::None;
    std::string reset = styled ? sgr_reset() : std::string{};

    // Buffer one logical line, then flush with `prefix` + line content + '\n'.
    std::string line;
    auto flush_line = [&]() {
        out.append(prefix.data(), prefix.size());
        out.append(line);
        if (styled)
            out.append(reset);
        out.push_back('\n');
        line.clear();
    };

    // Coalesce consecutive same-kind tokens so multi-char runs (== != -> => ::,
    // multi-line comments, adjacent keywords sharing a colour) emit a single
    // SGR pair instead of one pair per character. We hold the pending run in
    // `run` and flush it when the kind changes, a newline is emitted, or the
    // input ends.
    TokenKind run_kind = TokenKind::Text;
    std::string run_text;
    auto flush_run = [&]() {
        if (run_text.empty())
            return;
        if (styled && run_kind != TokenKind::Text) {
            line.append(sgr(style_for(run_kind), color));
            line.append(run_text);
            line.append(reset);
        } else {
            line.append(run_text);
        }
        run_text.clear();
    };

    auto sink = [&](TokenKind kind, std::string_view text) {
        if (text.empty())
            return;
        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\n') {
                if (i > start) {
                    if (kind != run_kind) {
                        flush_run();
                        run_kind = kind;
                    }
                    run_text.append(text.data() + start, i - start);
                }
                flush_run();
                flush_line();
                start = i + 1;
            }
        }
        if (start < text.size()) {
            if (kind != run_kind) {
                flush_run();
                run_kind = kind;
            }
            run_text.append(text.data() + start, text.size() - start);
        }
    };

    if (!fn || lang == Language::PlainText) {
        sink(TokenKind::Text, source);
    } else {
        fn(source, sink);
    }
    // Trailing partial line (no final newline in source).
    flush_run();
    if (!line.empty()) {
        out.append(prefix.data(), prefix.size());
        out.append(line);
        if (styled)
            out.append(reset);
        out.push_back('\n');
    }
}

}  // namespace rcat
