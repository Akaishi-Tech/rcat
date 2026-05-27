// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

// C-family tokeniser: every curly-brace language with // line comments and
// /* … */ block comments. Variations across the dialects are kept in
// per-language keyword/type/builtin tables; the scanner itself is shared.

#include "lang_common.hpp"

#include <cstring>
#include <string_view>

namespace rcat::lang {

namespace {

// Each "set" is a NULL-terminated array of C strings.
using KWSet = const char* const*;

struct Dialect {
    KWSet keywords;
    KWSet types;
    KWSet builtins;        // language-level functions/constants/literals
    bool  hash_preproc;    // C/C++/ObjC/PHP have # directives at BOL
    bool  triple_quote;    // not commonly C-family but kept for future
    bool  backtick_string; // Groovy, some others
    bool  raw_string_prefix; // C++ R"(...)", Rust r#"..."#, etc.
    bool  attribute_at;    // @decorator (Java/TS/Dart/PHP attributes)
    bool  attribute_hash;  // #[…] (Rust attributes)
};

// ---- keyword tables (alphabetical, NULL-terminated) ---------------------

const char* const kw_c[] = {
    "auto","break","case","const","continue","default","do","else","enum",
    "extern","for","goto","if","inline","register","restrict","return","sizeof",
    "static","struct","switch","typedef","union","volatile","while",
    "_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_Imaginary",
    "_Noreturn","_Static_assert","_Thread_local",
    nullptr};
const char* const ty_c[] = {
    "char","double","float","int","long","short","signed","unsigned","void",
    "size_t","ssize_t","ptrdiff_t","intptr_t","uintptr_t","bool",
    "int8_t","int16_t","int32_t","int64_t",
    "uint8_t","uint16_t","uint32_t","uint64_t",
    "FILE","NULL",
    nullptr};
const char* const bi_c[] = {
    "true","false","NULL","stdin","stdout","stderr","errno",
    nullptr};

const char* const kw_cpp[] = {
    "alignas","alignof","and","and_eq","asm","auto","bitand","bitor","break",
    "case","catch","class","compl","concept","const","consteval","constexpr",
    "constinit","const_cast","continue","co_await","co_return","co_yield",
    "decltype","default","delete","do","dynamic_cast","else","enum","explicit",
    "export","extern","for","friend","goto","if","inline","mutable","namespace",
    "new","noexcept","not","not_eq","operator","or","or_eq","private",
    "protected","public","register","reinterpret_cast","requires","return",
    "sizeof","static","static_assert","static_cast","struct","switch",
    "template","this","thread_local","throw","try","typedef","typeid",
    "typename","union","using","virtual","volatile","while","xor","xor_eq",
    nullptr};
const char* const ty_cpp[] = {
    "auto","bool","char","char8_t","char16_t","char32_t","double","float",
    "int","long","short","signed","unsigned","void","wchar_t",
    "size_t","ssize_t","ptrdiff_t","intmax_t","uintmax_t","nullptr_t",
    "int8_t","int16_t","int32_t","int64_t",
    "uint8_t","uint16_t","uint32_t","uint64_t",
    "string","string_view","vector","map","set","array","optional","variant",
    "unique_ptr","shared_ptr","weak_ptr","function","span",
    nullptr};
const char* const bi_cpp[] = {
    "true","false","nullptr","NULL","this","std",
    nullptr};

const char* const kw_java[] = {
    "abstract","assert","break","case","catch","class","const","continue",
    "default","do","else","enum","extends","final","finally","for","goto",
    "if","implements","import","instanceof","interface","native","new",
    "non-sealed","package","permits","private","protected","public","record",
    "return","sealed","static","strictfp","super","switch","synchronized",
    "this","throw","throws","transient","try","var","volatile","while","yield",
    nullptr};
const char* const ty_java[] = {
    "boolean","byte","char","double","float","int","long","short","void",
    "Boolean","Byte","Character","Double","Float","Integer","Long","Short",
    "String","Object","Number","List","Map","Set","Optional","Stream",
    nullptr};
const char* const bi_java[] = {
    "true","false","null","System","Math",
    nullptr};

const char* const kw_js[] = {
    "as","async","await","break","case","catch","class","const","continue",
    "debugger","default","delete","do","else","export","extends","finally",
    "for","from","function","get","if","import","in","instanceof","let","new",
    "of","return","set","static","super","switch","this","throw","try","typeof",
    "var","void","while","with","yield",
    nullptr};
const char* const ty_js[] = {
    "Array","Boolean","Date","Error","Function","Map","Number","Object",
    "Promise","RegExp","Set","String","Symbol","WeakMap","WeakSet",
    "BigInt","Int8Array","Uint8Array","Uint8ClampedArray","Int16Array",
    "Uint16Array","Int32Array","Uint32Array","Float32Array","Float64Array",
    nullptr};
const char* const bi_js[] = {
    "true","false","null","undefined","NaN","Infinity","console","window",
    "document","globalThis","require","module","exports","process",
    nullptr};

const char* const kw_ts[] = {
    "abstract","any","as","asserts","async","await","break","case","catch",
    "class","const","constructor","continue","debugger","declare","default",
    "delete","do","else","enum","export","extends","finally","for","from",
    "function","get","if","implements","import","in","infer","instanceof",
    "interface","is","keyof","let","module","namespace","never","new","of",
    "package","private","protected","public","readonly","return","satisfies",
    "set","static","super","switch","this","throw","try","type","typeof",
    "unique","unknown","var","void","while","with","yield",
    nullptr};
const char* const ty_ts[] = {
    "any","boolean","bigint","never","number","object","string","symbol",
    "undefined","unknown","void","Array","Promise","Record","Partial",
    "Readonly","Required","Pick","Omit","Map","Set",
    nullptr};
// bi_js reused

const char* const kw_csharp[] = {
    "abstract","add","alias","as","ascending","async","await","base","break",
    "by","case","catch","checked","class","const","continue","default",
    "delegate","descending","do","dynamic","else","enum","equals","event",
    "explicit","extern","false","finally","fixed","for","foreach","from",
    "get","global","goto","group","if","implicit","in","init","interface",
    "internal","into","is","join","let","lock","nameof","namespace","new",
    "not","null","object","on","operator","orderby","out","override","params",
    "partial","private","protected","public","readonly","record","ref","remove",
    "return","sealed","select","set","sizeof","stackalloc","static","struct",
    "switch","this","throw","true","try","typeof","unchecked","unsafe","using",
    "value","var","virtual","void","volatile","when","where","while","with",
    "yield",
    nullptr};
const char* const ty_csharp[] = {
    "bool","byte","char","decimal","double","float","int","long","object",
    "sbyte","short","string","uint","ulong","ushort","void",
    "Boolean","Byte","Char","Decimal","Double","Int16","Int32","Int64",
    "Object","SByte","Single","String","UInt16","UInt32","UInt64",
    "List","Dictionary","IEnumerable","Task",
    nullptr};
const char* const bi_csharp[] = {
    "true","false","null","this","base","Console",
    nullptr};

const char* const kw_go[] = {
    "break","case","chan","const","continue","default","defer","else",
    "fallthrough","for","func","go","goto","if","import","interface","map",
    "package","range","return","select","struct","switch","type","var",
    nullptr};
const char* const ty_go[] = {
    "bool","byte","complex64","complex128","error","float32","float64",
    "int","int8","int16","int32","int64","rune","string","uint","uint8",
    "uint16","uint32","uint64","uintptr","any",
    nullptr};
const char* const bi_go[] = {
    "true","false","nil","iota","append","cap","close","complex","copy",
    "delete","imag","len","make","new","panic","print","println","real",
    "recover","min","max","clear",
    nullptr};

const char* const kw_rust[] = {
    "as","async","await","break","const","continue","crate","do","dyn","else",
    "enum","extern","false","fn","for","if","impl","in","let","loop","match",
    "mod","move","mut","pub","ref","return","self","Self","static","struct",
    "super","trait","true","try","type","union","unsafe","use","where","while",
    "yield",
    nullptr};
const char* const ty_rust[] = {
    "bool","char","f32","f64","i8","i16","i32","i64","i128","isize",
    "str","u8","u16","u32","u64","u128","usize",
    "String","Vec","Box","Rc","Arc","Option","Result","HashMap","HashSet",
    "BTreeMap","BTreeSet",
    nullptr};
const char* const bi_rust[] = {
    "true","false","Some","None","Ok","Err","Self","self","println","print",
    "eprintln","eprint","format","vec","assert","assert_eq","assert_ne","dbg",
    "panic","todo","unimplemented","unreachable","write","writeln",
    nullptr};

const char* const kw_kotlin[] = {
    "abstract","actual","annotation","as","break","by","catch","class",
    "companion","const","constructor","continue","crossinline","data","do",
    "dynamic","else","enum","expect","external","false","field","file","final",
    "finally","for","fun","get","if","import","in","infix","init","inline",
    "inner","interface","internal","is","lateinit","noinline","null","object",
    "open","operator","out","override","package","param","private","property",
    "protected","public","receiver","reified","return","sealed","set","setparam",
    "super","suspend","tailrec","this","throw","true","try","typealias",
    "typeof","val","var","vararg","when","where","while","yield",
    nullptr};
const char* const ty_kotlin[] = {
    "Any","Array","Boolean","Byte","Char","Double","Float","Int","Long",
    "Nothing","Short","String","Unit","List","Map","Set","Pair","Triple",
    nullptr};
const char* const bi_kotlin[] = {
    "true","false","null","this","super","println","print","arrayOf","listOf",
    "mapOf","setOf","mutableListOf","mutableMapOf","mutableSetOf",
    nullptr};

const char* const kw_scala[] = {
    "abstract","case","catch","class","def","do","else","enum","export",
    "extends","false","final","finally","for","forSome","given","if",
    "implicit","import","lazy","macro","match","new","null","object",
    "override","package","private","protected","return","sealed","super",
    "then","this","throw","trait","try","true","type","using","val","var",
    "while","with","yield",
    nullptr};
const char* const ty_scala[] = {
    "Any","AnyRef","AnyVal","Boolean","Byte","Char","Double","Float","Int",
    "Long","Nothing","Null","Short","Unit","String","List","Map","Set",
    "Option","Some","None","Either","Future","Seq","Vector",
    nullptr};
const char* const bi_scala[] = {
    "true","false","null","this","super","println","print",
    nullptr};

const char* const kw_swift[] = {
    "actor","any","as","associatedtype","async","await","borrowing","break",
    "case","catch","class","consume","consuming","continue","default","defer",
    "deinit","do","else","enum","extension","fallthrough","false","fileprivate",
    "for","func","guard","if","import","in","indirect","init","inout","internal",
    "is","let","nonisolated","open","operator","precedencegroup","private",
    "protocol","public","repeat","rethrows","return","self","Self","static",
    "struct","subscript","super","switch","throw","throws","true","try","typealias",
    "var","where","while",
    nullptr};
const char* const ty_swift[] = {
    "Array","Bool","Character","Dictionary","Double","Float","Int","Int8",
    "Int16","Int32","Int64","Optional","Set","String","UInt","UInt8","UInt16",
    "UInt32","UInt64","Void","Any","AnyObject",
    nullptr};
const char* const bi_swift[] = {
    "true","false","nil","self","super","print","Self","throws","rethrows",
    nullptr};

const char* const kw_dart[] = {
    "abstract","as","assert","async","await","base","break","case","catch",
    "class","const","continue","covariant","default","deferred","do","dynamic",
    "else","enum","export","extends","extension","external","factory","false",
    "final","finally","for","Function","get","hide","if","implements","import",
    "in","interface","is","late","library","mixin","new","null","of","on",
    "operator","part","required","rethrow","return","sealed","set","show",
    "static","super","switch","sync","this","throw","true","try","typedef",
    "var","void","when","while","with","yield",
    nullptr};
const char* const ty_dart[] = {
    "bool","double","dynamic","int","num","String","List","Map","Set","Object",
    "Iterable","Future","Stream","Symbol","void","Null",
    nullptr};
const char* const bi_dart[] = {
    "true","false","null","this","super","print",
    nullptr};

const char* const kw_objc[] = {
    "auto","break","case","const","continue","default","do","else","enum",
    "extern","for","goto","if","inline","register","restrict","return","sizeof",
    "static","struct","switch","typedef","union","volatile","while",
    "@interface","@implementation","@protocol","@end","@class","@property",
    "@synthesize","@dynamic","@selector","@encode","@autoreleasepool","@try",
    "@catch","@finally","@throw","@synchronized","self","super","id","SEL",
    "BOOL","YES","NO","nil","Nil","IBOutlet","IBAction",
    nullptr};
const char* const ty_objc[] = {
    "char","double","float","int","long","short","signed","unsigned","void",
    "NSString","NSArray","NSDictionary","NSSet","NSNumber","NSInteger",
    "NSUInteger","CGFloat","NSObject","NSError",
    nullptr};
const char* const bi_objc[] = {
    "true","false","YES","NO","nil","Nil","NULL","self","super",
    nullptr};

const char* const kw_php[] = {
    "abstract","and","array","as","break","callable","case","catch","class",
    "clone","const","continue","declare","default","die","do","echo","else",
    "elseif","empty","enddeclare","endfor","endforeach","endif","endswitch",
    "endwhile","enum","extends","final","finally","fn","for","foreach",
    "function","global","goto","if","implements","include","include_once",
    "instanceof","insteadof","interface","isset","list","match","namespace",
    "new","or","print","private","protected","public","readonly","require",
    "require_once","return","static","switch","throw","trait","try","unset",
    "use","var","while","xor","yield",
    nullptr};
const char* const ty_php[] = {
    "array","bool","callable","float","int","iterable","mixed","never",
    "object","self","static","string","void","parent",
    nullptr};
const char* const bi_php[] = {
    "true","false","null","this","echo","print","isset","empty","var_dump",
    "count","strlen","is_array","is_string","is_int","is_bool","is_null",
    nullptr};

const char* const kw_groovy[] = {
    "abstract","as","assert","break","case","catch","class","const","continue",
    "def","default","do","else","enum","extends","false","final","finally",
    "for","goto","if","implements","import","in","instanceof","interface",
    "native","new","null","package","private","protected","public","return",
    "static","strictfp","super","switch","synchronized","this","throw","throws",
    "trait","transient","true","try","var","void","volatile","while",
    nullptr};
const char* const ty_groovy[] = {
    "boolean","byte","char","double","float","int","long","short","void",
    "String","Object","List","Map","Set","def",
    nullptr};
const char* const bi_groovy[] = {
    "true","false","null","this","super","println","print",
    nullptr};

const char* const kw_zig[] = {
    "addrspace","align","allowzero","and","anyframe","anytype","asm","async",
    "await","break","callconv","catch","comptime","const","continue","defer",
    "else","enum","errdefer","error","export","extern","fn","for","if","inline",
    "linksection","noalias","noinline","nosuspend","opaque","or","orelse",
    "packed","pub","resume","return","struct","suspend","switch","test",
    "threadlocal","try","union","unreachable","usingnamespace","var","volatile",
    "while",
    nullptr};
const char* const ty_zig[] = {
    "bool","void","noreturn","type","anytype","anyerror","anyframe","comptime_int",
    "comptime_float","f16","f32","f64","f80","f128","i8","i16","i32","i64",
    "i128","isize","u8","u16","u32","u64","u128","usize","c_short","c_int",
    "c_long","c_longlong","c_void",
    nullptr};
const char* const bi_zig[] = {
    "true","false","null","undefined","this",
    nullptr};

const char* const kw_v[] = {
    "as","asm","assert","atomic","break","const","continue","defer","else",
    "enum","fn","for","go","goto","if","import","in","interface","is","isreftype",
    "lock","match","module","mut","none","or","pub","return","rlock","select",
    "shared","sizeof","spawn","static","struct","type","typeof","union","unsafe",
    "volatile",
    nullptr};
const char* const ty_v[] = {
    "bool","string","i8","i16","int","i64","u8","u16","u32","u64","f32","f64",
    "rune","isize","usize","voidptr","byteptr","charptr","any","map","array",
    nullptr};
const char* const bi_v[] = {
    "true","false","none","println","print","eprintln","exit",
    nullptr};

const char* const kw_nim[] = {
    "addr","and","as","asm","bind","block","break","case","cast","concept",
    "const","continue","converter","defer","discard","distinct","div","do",
    "elif","else","end","enum","except","export","finally","for","from","func",
    "if","import","in","include","interface","is","isnot","iterator","let",
    "macro","method","mixin","mod","nil","not","notin","object","of","or","out",
    "proc","ptr","raise","ref","return","shl","shr","static","template","try",
    "tuple","type","using","var","when","while","xor","yield",
    nullptr};
const char* const ty_nim[] = {
    "int","int8","int16","int32","int64","uint","uint8","uint16","uint32",
    "uint64","float","float32","float64","char","string","bool","cstring",
    "seq","array","openArray","void","range","pointer","object",
    nullptr};
const char* const bi_nim[] = {
    "true","false","nil","echo","result","new","len","add","inc","dec","high",
    "low",
    nullptr};

const char* const kw_crystal[] = {
    "abstract","alias","annotation","as","as?","asm","begin","break","case",
    "class","def","do","else","elsif","end","ensure","enum","extend","false",
    "for","fun","if","in","include","instance_sizeof","is_a?","lib","macro",
    "module","next","nil","of","out","pointerof","private","protected","require",
    "rescue","return","select","self","sizeof","struct","super","then","true",
    "type","typeof","union","unless","until","verbatim","when","while","with",
    "yield","__DIR__","__FILE__","__LINE__","__END_LINE__",
    nullptr};
const char* const ty_crystal[] = {
    "Bool","Char","Float32","Float64","Int8","Int16","Int32","Int64","UInt8",
    "UInt16","UInt32","UInt64","String","Symbol","Array","Hash","Set","Nil",
    "Object","Tuple","NamedTuple","Range","Number","Pointer","Void",
    nullptr};
const char* const bi_crystal[] = {
    "true","false","nil","self","puts","print","p","pp","raise","loop",
    nullptr};

const char* const kw_d[] = {
    "abstract","alias","align","asm","assert","auto","body","break","case",
    "cast","catch","class","const","continue","debug","default","delegate",
    "delete","deprecated","do","else","enum","export","extern","false","final",
    "finally","for","foreach","foreach_reverse","function","goto","if",
    "immutable","import","in","inout","interface","invariant","is","lazy",
    "macro","mixin","module","new","nothrow","null","out","override","package",
    "pragma","private","protected","public","pure","ref","return","scope",
    "shared","static","struct","super","switch","synchronized","template",
    "this","throw","true","try","typeid","typeof","union","unittest","version",
    "virtual","volatile","while","with","__gshared","__traits","__vector",
    "__parameters",
    nullptr};
const char* const ty_d[] = {
    "bool","byte","cdouble","cent","cfloat","char","creal","dchar","double",
    "float","idouble","ifloat","int","ireal","long","real","short","ubyte",
    "ucent","uint","ulong","ushort","void","wchar","string","wstring","dstring",
    "size_t","ptrdiff_t",
    nullptr};
const char* const bi_d[] = {
    "true","false","null","this","super","writeln","write","writefln","writef",
    nullptr};

// ---- shared scanner -----------------------------------------------------

void scan(std::string_view s, const TokenSink& emit, const Dialect& d) {
    size_t i = 0, n = s.size();
    bool at_bol = true;
    while (i < n) {
        char c = s[i];

        if (c == '\n') {
            emit(TokenKind::Text, s.substr(i, 1));
            ++i;
            at_bol = true;
            continue;
        }
        if (is_space(c)) {
            size_t j = i;
            while (j < n && is_space(s[j])) ++j;
            emit(TokenKind::Text, s.substr(i, j - i));
            i = j;
            continue;
        }

        // preprocessor (C/C++/ObjC/PHP): a # in column 0 (or after only
        // whitespace) takes the rest of the line. PHP's tag <?php is handled
        // by passthrough since it starts with '<'.
        if (at_bol && d.hash_preproc && c == '#') {
            size_t j = scan_line(s, i);
            emit(TokenKind::Preprocessor, s.substr(i, j - i));
            i = j;
            continue;
        }

        // line comment //
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            size_t j = scan_line(s, i);
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }
        // block comment /* ... */
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '/')) ++j;
            j = j + 1 < n ? j + 2 : n;
            emit_split_newlines(emit, TokenKind::Comment, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }

        // attribute #[...]
        if (d.attribute_hash && c == '#' && i + 1 < n && s[i + 1] == '[') {
            size_t j = i + 2;
            int depth = 1;
            while (j < n && depth > 0) {
                if (s[j] == '[') ++depth;
                else if (s[j] == ']') --depth;
                ++j;
            }
            emit(TokenKind::Decorator, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }

        // decorator @ident (Java/TS/Dart/PHP)
        if (d.attribute_at && c == '@' && i + 1 < n
            && (is_ident_start(s[i + 1]) || s[i + 1] == '_')) {
            size_t j = scan_ident(s, i + 1);
            emit(TokenKind::Decorator, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }

        // raw string prefix
        if (d.raw_string_prefix && (c == 'r' || c == 'R')
            && i + 1 < n
            && (s[i + 1] == '"' || s[i + 1] == '#')) {
            size_t j = i + 1;
            int hashes = 0;
            while (j < n && s[j] == '#') { ++hashes; ++j; }
            if (j < n && s[j] == '"') {
                size_t start = i;
                ++j;
                while (j < n) {
                    if (s[j] == '"') {
                        bool ok = true;
                        for (int k = 0; k < hashes; ++k) {
                            if (j + 1 + k >= n || s[j + 1 + k] != '#') {
                                ok = false;
                                break;
                            }
                        }
                        if (ok) { j += 1 + hashes; break; }
                    }
                    ++j;
                }
                emit_split_newlines(emit, TokenKind::String,
                                    s.substr(start, j - start));
                i = j;
                at_bol = false;
                continue;
            }
        }

        // strings " " and ' '
        if (c == '"' || c == '\'') {
            auto r = scan_simple_string(s, i, c, /*allow_newline*/ false);
            emit_split_newlines(emit, TokenKind::String, s.substr(i, r.end - i));
            i = r.end;
            at_bol = false;
            continue;
        }
        // template literal `…` (JS/TS/Groovy)
        if (d.backtick_string && c == '`') {
            size_t j = i + 1;
            while (j < n && s[j] != '`') {
                if (s[j] == '\\' && j + 1 < n) j += 2;
                else ++j;
            }
            if (j < n) ++j;
            emit_split_newlines(emit, TokenKind::String, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }

        // numbers
        if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1]))) {
            size_t j = scan_number(s, i);
            emit(TokenKind::Number, s.substr(i, j - i));
            i = j;
            at_bol = false;
            continue;
        }

        // identifiers / keywords / type / function-call detection
        if (is_ident_start(c)) {
            size_t j = scan_ident(s, i);
            std::string_view word = s.substr(i, j - i);

            // upper-case → constant heuristic (and not a single letter)
            bool all_upper = word.size() > 1;
            for (char ch : word) {
                if (!((ch >= 'A' && ch <= 'Z') || ch == '_' || is_digit(ch))) {
                    all_upper = false;
                    break;
                }
            }

            TokenKind kk = TokenKind::Text;
            if (in_keyword_set(word, d.keywords)) kk = TokenKind::Keyword;
            else if (in_keyword_set(word, d.types)) kk = TokenKind::BuiltinType;
            else if (in_keyword_set(word, d.builtins)) kk = TokenKind::Builtin;
            else if (all_upper) kk = TokenKind::Constant;
            else {
                size_t k = j;
                while (k < n && is_space(s[k])) ++k;
                if (k < n && s[k] == '(') kk = TokenKind::Function;
            }
            emit(kk, word);
            i = j;
            at_bol = false;
            continue;
        }

        // punctuation / operators
        static const char* punct = "{}[]();,";
        if (std::strchr(punct, c)) {
            emit(TokenKind::Punctuation, s.substr(i, 1));
            ++i;
            at_bol = false;
            continue;
        }
        emit(TokenKind::Operator, s.substr(i, 1));
        ++i;
        at_bol = false;
    }
}

const Dialect D_C       = {kw_c,        ty_c,       bi_c,       true,  false, false, false, false, false};
const Dialect D_Cpp     = {kw_cpp,      ty_cpp,     bi_cpp,     true,  false, false, true,  false, false};
const Dialect D_Java    = {kw_java,     ty_java,    bi_java,    false, false, false, false, true,  false};
const Dialect D_Js      = {kw_js,       ty_js,      bi_js,      false, false, true,  false, true,  false};
const Dialect D_Ts      = {kw_ts,       ty_ts,      bi_js,      false, false, true,  false, true,  false};
const Dialect D_Cs      = {kw_csharp,   ty_csharp,  bi_csharp,  false, false, false, false, true,  false};
const Dialect D_Go      = {kw_go,       ty_go,      bi_go,      false, false, true,  false, false, false};
const Dialect D_Rust    = {kw_rust,     ty_rust,    bi_rust,    false, false, false, true,  false, true};
const Dialect D_Kotlin  = {kw_kotlin,   ty_kotlin,  bi_kotlin,  false, false, false, false, true,  false};
const Dialect D_Scala   = {kw_scala,    ty_scala,   bi_scala,   false, false, true,  false, true,  false};
const Dialect D_Swift   = {kw_swift,    ty_swift,   bi_swift,   true,  false, false, false, true,  false};
const Dialect D_Dart    = {kw_dart,     ty_dart,    bi_dart,    false, false, false, true,  true,  false};
const Dialect D_ObjC    = {kw_objc,     ty_objc,    bi_objc,    true,  false, false, false, true,  false};
const Dialect D_Php     = {kw_php,      ty_php,     bi_php,     false, false, false, false, true,  false};
const Dialect D_Groovy  = {kw_groovy,   ty_groovy,  bi_groovy,  false, false, true,  false, true,  false};
const Dialect D_Zig     = {kw_zig,      ty_zig,     bi_zig,     false, false, false, false, false, false};
const Dialect D_V       = {kw_v,        ty_v,       bi_v,       false, false, true,  false, false, false};
const Dialect D_Nim     = {kw_nim,      ty_nim,     bi_nim,     false, false, false, false, false, false};
const Dialect D_Crystal = {kw_crystal,  ty_crystal, bi_crystal, false, false, false, false, false, false};
const Dialect D_D       = {kw_d,        ty_d,       bi_d,       false, false, false, false, true,  false};

} // namespace

void tokenise_c        (std::string_view s, const TokenSink& e) { scan(s, e, D_C);       }
void tokenise_cpp      (std::string_view s, const TokenSink& e) { scan(s, e, D_Cpp);     }
void tokenise_java     (std::string_view s, const TokenSink& e) { scan(s, e, D_Java);    }
void tokenise_javascript(std::string_view s,const TokenSink& e) { scan(s, e, D_Js);      }
void tokenise_typescript(std::string_view s,const TokenSink& e) { scan(s, e, D_Ts);      }
void tokenise_csharp   (std::string_view s, const TokenSink& e) { scan(s, e, D_Cs);      }
void tokenise_go       (std::string_view s, const TokenSink& e) { scan(s, e, D_Go);      }
void tokenise_rust     (std::string_view s, const TokenSink& e) { scan(s, e, D_Rust);    }
void tokenise_kotlin   (std::string_view s, const TokenSink& e) { scan(s, e, D_Kotlin);  }
void tokenise_scala    (std::string_view s, const TokenSink& e) { scan(s, e, D_Scala);   }
void tokenise_swift    (std::string_view s, const TokenSink& e) { scan(s, e, D_Swift);   }
void tokenise_dart     (std::string_view s, const TokenSink& e) { scan(s, e, D_Dart);    }
void tokenise_objectivec(std::string_view s,const TokenSink& e) { scan(s, e, D_ObjC);    }
void tokenise_php      (std::string_view s, const TokenSink& e) { scan(s, e, D_Php);     }
void tokenise_groovy   (std::string_view s, const TokenSink& e) { scan(s, e, D_Groovy);  }
void tokenise_zig      (std::string_view s, const TokenSink& e) { scan(s, e, D_Zig);     }
void tokenise_v        (std::string_view s, const TokenSink& e) { scan(s, e, D_V);       }
void tokenise_nim      (std::string_view s, const TokenSink& e) { scan(s, e, D_Nim);     }
void tokenise_crystal  (std::string_view s, const TokenSink& e) { scan(s, e, D_Crystal); }
void tokenise_d        (std::string_view s, const TokenSink& e) { scan(s, e, D_D);       }

} // namespace rcat::lang
