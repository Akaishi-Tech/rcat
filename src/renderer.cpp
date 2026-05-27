// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "renderer.hpp"

#include "highlight.hpp"
#include "i18n.hpp"
#include "image.hpp"
#include "md4c.h"
#include "wrap.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rcat {

namespace {

struct ListCtx {
    bool ordered;
    int next_num;
    int indent;           // column where paragraph content starts
    std::string pending;  // marker for the next P (e.g. "* " / "1. ")
};

struct TableRow {
    std::vector<std::string> cells;  // rendered inline (may contain SGR)
    bool is_header = false;
};

struct TableState {
    std::vector<TableRow> rows;
    std::vector<MD_ALIGN> aligns;
    bool in_header = false;
};

struct RS {
    RenderOptions opts;
    std::string out;

    // Inline accumulator
    std::string inline_buf;
    bool accumulating = false;

    // Block context
    int quote_depth = 0;
    std::vector<ListCtx> lists;
    int heading_level = 0;
    bool in_code_block = false;
    std::string code_lang;
    // Last fenced-block language seen; used to highlight subsequent inline
    // `code` spans. Cleared by detect_language() not matching.
    std::string last_code_lang;

    // Inline-code capture: when we are inside MD_SPAN_CODE, text_cb pushes
    // into inline_code_buf instead of inline_buf so we can tokenise the
    // whole span at close-time.
    std::string inline_code_buf;
    bool in_inline_code = false;

    // Span / styling context
    std::vector<Style> style_stack;
    std::vector<std::string> link_urls;  // for OSC 8 close / fallback
    bool in_image = false;
    std::string pending_image_url;

    // Tables
    TableState table;
    bool in_table = false;

    // Images discovered inline; rendered after the enclosing block finishes
    // so they appear below the paragraph that referenced them.
    struct PendingImage {
        std::string alt;
        std::string path;  // absolute or unresolved-remote
        bool remote;
    };
    std::vector<PendingImage> pending_images;

    // helpers --------------------------------------------------------------

    bool color() const { return opts.caps.color != ColorMode::None; }

    std::string sgr_open(const Style& s) const {
        return color() ? sgr(s, opts.caps.color) : std::string{};
    }
    std::string sgr_off() const { return color() ? sgr_reset() : std::string{}; }

    Style top_style() const { return style_stack.empty() ? Style{} : style_stack.back(); }

    void push_style(Style s) {
        style_stack.push_back(s);
        if (accumulating)
            inline_buf += sgr_open(s);
    }
    void pop_style() {
        if (!style_stack.empty())
            style_stack.pop_back();
        if (accumulating) {
            inline_buf += sgr_off();
            if (!style_stack.empty())
                inline_buf += sgr_open(style_stack.back());
        }
    }

    std::string quote_prefix() const {
        std::string p;
        for (int i = 0; i < quote_depth; ++i) {
            p += opts.caps.unicode ? "\xE2\x94\x82 " : "| ";
        }
        return p;
    }

    // Prefix every line of a non-paragraph block (code, hr, table) must
    // share: quote bars + indentation imposed by enclosing lists.
    std::string block_prefix() const {
        std::string p = quote_prefix();
        if (!lists.empty())
            p.append(lists.back().indent, ' ');
        return p;
    }

    int content_columns() const { return std::max(10, opts.caps.columns); }
};

// ---------------------------------------------------------------------------
// Image helpers

bool looks_remote(std::string_view src) {
    static const std::string_view schemes[] = {"http://", "https://", "data:", "ftp://", "//"};
    for (auto s : schemes) {
        if (src.size() >= s.size() && std::equal(s.begin(), s.end(), src.begin())) {
            return true;
        }
    }
    return false;
}

bool path_is_absolute(std::string_view p) {
    return !p.empty() && p.front() == '/';
}

std::string resolve_local_path(std::string_view src, std::string_view doc_dir) {
    std::string s(src);
    // Strip a leading "file://" if present.
    constexpr std::string_view file_scheme = "file://";
    if (s.size() >= file_scheme.size() &&
        std::equal(file_scheme.begin(), file_scheme.end(), s.begin())) {
        s.erase(0, file_scheme.size());
    }
    if (path_is_absolute(s) || doc_dir.empty())
        return s;
    std::string out(doc_dir);
    if (!out.empty() && out.back() != '/')
        out += '/';
    out.append(s);
    return out;
}

// Append `rendered` (multi-line, may contain ANSI) to `out`, prefixing
// every line with `prefix`. Does NOT add a leading or trailing blank line.
void append_prefixed(std::string& out, std::string_view rendered, std::string_view prefix) {
    size_t start = 0;
    for (size_t i = 0; i <= rendered.size(); ++i) {
        if (i == rendered.size() || rendered[i] == '\n') {
            out.append(prefix.data(), prefix.size());
            out.append(rendered.data() + start, i - start);
            out.push_back('\n');
            start = i + 1;
        }
    }
    // If `rendered` ended with '\n' we appended an extra empty prefixed line —
    // trim it back off so we don't introduce a spurious blank row.
    if (!rendered.empty() && rendered.back() == '\n' && out.size() >= prefix.size() + 1) {
        out.resize(out.size() - prefix.size() - 1);
    }
}

void drain_pending_images(RS& st) {
    if (st.pending_images.empty())
        return;
    auto images = std::move(st.pending_images);
    st.pending_images.clear();

    std::string prefix = st.block_prefix();
    int max_w = std::max(1, st.content_columns() - (int)display_width(prefix));
    int max_h = std::max(1, st.opts.image_max_height);

    for (auto& img : images) {
        bool rendered = false;
        if (!st.opts.no_images && !img.path.empty()) {
            ImageRenderResult res;
            if (img.remote) {
                if (st.opts.allow_web) {
                    auto fetched =
                        fetch_url(img.path, st.opts.web_timeout_seconds, st.opts.web_max_bytes);
                    if (fetched.success && !fetched.bytes.empty()) {
                        res = render_image_bytes(
                            reinterpret_cast<const unsigned char*>(fetched.bytes.data()),
                            fetched.bytes.size(), max_w, max_h, st.opts.caps.color);
                    }
                }
            } else {
                res = render_image_file(img.path, max_w, max_h, st.opts.caps.color);
            }
            if (res.success && !res.output.empty()) {
                append_prefixed(st.out, res.output, prefix);
                st.out += "\n";
                rendered = true;
            }
        }
        if (!rendered) {
            Style s;
            s.dim = true;
            std::string line = prefix;
            line += st.sgr_open(s);
            line += "[";
            line += _("image: ");
            if (!img.alt.empty()) {
                line += img.alt;
                line += " — ";
            }
            line += img.path.empty() ? img.alt : img.path;
            line += "]";
            line += st.sgr_off();
            line += "\n";
            st.out += line;
        }
    }
    if (st.lists.empty() && st.quote_depth == 0)
        st.out += "\n";
}

// ---------------------------------------------------------------------------
// Helpers to append plain text into the inline buffer, escaping nothing —
// md4c hands us the original UTF-8 bytes.

void append_text(RS& st, const char* text, MD_SIZE size) {
    st.inline_buf.append(text, size);
}

// ---------------------------------------------------------------------------
// Emit a finished block (paragraph or heading) by computing its prefixes
// and word-wrapping the inline buffer.

void emit_block(RS& st, std::string_view content) {
    std::string qp = st.quote_prefix();
    int width = st.content_columns();

    if (!st.lists.empty()) {
        auto& lc = st.lists.back();
        int indent = lc.indent;
        std::string cont_prefix = qp + std::string(indent, ' ');
        std::string first_prefix;
        if (!lc.pending.empty()) {
            int marker_w = display_width(lc.pending);
            int pad = std::max(0, indent - marker_w);
            first_prefix = qp + std::string(pad, ' ') + lc.pending;
            lc.pending.clear();
        } else {
            first_prefix = cont_prefix;
        }
        std::string style_open =
            st.color() && !st.style_stack.empty() ? st.sgr_open(st.top_style()) : std::string{};
        emit_wrapped(st.out, content, first_prefix, cont_prefix, width, style_open);
    } else {
        std::string style_open =
            st.color() && !st.style_stack.empty() ? st.sgr_open(st.top_style()) : std::string{};
        emit_wrapped(st.out, content, qp, qp, width, style_open);
    }
}

// Pad a styled cell to `width` cells; alignment per MD_ALIGN.
std::string pad_cell(std::string_view cell, int width, MD_ALIGN a) {
    int w = display_width(cell);
    int pad = std::max(0, width - w);
    std::string left, right;
    switch (a) {
    case MD_ALIGN_RIGHT:
        left.assign(pad, ' ');
        break;
    case MD_ALIGN_CENTER: {
        int l = pad / 2, r = pad - l;
        left.assign(l, ' ');
        right.assign(r, ' ');
        break;
    }
    default:
        right.assign(pad, ' ');
        break;
    }
    std::string s;
    s.append(left);
    s.append(cell);
    s.append(right);
    return s;
}

void render_table(RS& st) {
    auto& t = st.table;
    if (t.rows.empty())
        return;

    size_t cols = 0;
    for (auto& r : t.rows)
        cols = std::max(cols, r.cells.size());
    if (cols == 0)
        return;
    while (t.aligns.size() < cols)
        t.aligns.push_back(MD_ALIGN_DEFAULT);

    std::vector<int> widths(cols, 0);
    for (auto& r : t.rows) {
        for (size_t i = 0; i < r.cells.size(); ++i) {
            widths[i] = std::max(widths[i], display_width(r.cells[i]));
        }
    }
    for (auto& w : widths)
        if (w < 1)
            w = 1;

    const bool u = st.opts.caps.unicode;
    auto H = u ? "\xE2\x94\x80" : "-";   // ─
    auto V = u ? "\xE2\x94\x82" : "|";   // │
    auto TL = u ? "\xE2\x94\x8C" : "+";  // ┌
    auto TR = u ? "\xE2\x94\x90" : "+";  // ┐
    auto BL = u ? "\xE2\x94\x94" : "+";  // └
    auto BR = u ? "\xE2\x94\x98" : "+";  // ┘
    auto LM = u ? "\xE2\x94\x9C" : "+";  // ├
    auto RM = u ? "\xE2\x94\xA4" : "+";  // ┤
    auto TM = u ? "\xE2\x94\xAC" : "+";  // ┬
    auto BM = u ? "\xE2\x94\xB4" : "+";  // ┴
    auto XM = u ? "\xE2\x94\xBC" : "+";  // ┼

    auto repeat = [](const char* s, int n) {
        std::string r;
        for (int i = 0; i < n; ++i)
            r += s;
        return r;
    };

    auto sep = [&](const char* l, const char* m, const char* r) {
        std::string s = st.block_prefix();
        s += l;
        for (size_t i = 0; i < cols; ++i) {
            s += repeat(H, widths[i] + 2);
            s += (i + 1 == cols) ? r : m;
        }
        s += "\n";
        return s;
    };

    st.out += sep(TL, TM, TR);
    for (size_t ri = 0; ri < t.rows.size(); ++ri) {
        const auto& r = t.rows[ri];
        std::string line = st.block_prefix();
        line += V;
        for (size_t i = 0; i < cols; ++i) {
            line += ' ';
            std::string_view cell =
                i < r.cells.size() ? std::string_view(r.cells[i]) : std::string_view{};
            line += pad_cell(cell, widths[i], t.aligns[i]);
            line += ' ';
            line += V;
        }
        line += "\n";
        st.out += line;
        if (ri == 0 && r.is_header && t.rows.size() > 1) {
            st.out += sep(LM, XM, RM);
        }
    }
    st.out += sep(BL, BM, BR);
}

// ---------------------------------------------------------------------------
// md4c callbacks

int enter_block_cb(MD_BLOCKTYPE type, void* detail, void* userdata) {
    RS& st = *static_cast<RS*>(userdata);
    // Before opening any block other than DOC, flush any pending inline
    // content from an enclosing LI (md4c emits text directly inside LI for
    // "tight" lists — no inner P block — so we must capture it here and
    // emit it before a nested block starts).
    if (type != MD_BLOCK_DOC && st.accumulating && !st.inline_buf.empty()) {
        emit_block(st, st.inline_buf);
        st.inline_buf.clear();
        st.accumulating = false;
    }
    switch (type) {
    case MD_BLOCK_DOC:
        break;

    case MD_BLOCK_QUOTE:
        st.quote_depth++;
        break;

    case MD_BLOCK_UL: {
        ListCtx lc;
        lc.ordered = false;
        lc.next_num = 0;
        int base = st.lists.empty() ? 0 : st.lists.back().indent;
        lc.indent = base + 2;
        st.lists.push_back(lc);
        break;
    }
    case MD_BLOCK_OL: {
        auto* d = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
        ListCtx lc;
        lc.ordered = true;
        lc.next_num = d ? static_cast<int>(d->start) : 1;
        int base = st.lists.empty() ? 0 : st.lists.back().indent;
        lc.indent = base + 4;
        st.lists.push_back(lc);
        break;
    }
    case MD_BLOCK_LI: {
        auto* d = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
        if (!st.lists.empty()) {
            auto& lc = st.lists.back();
            if (d && d->is_task) {
                const char* box = d->task_mark == 'x' || d->task_mark == 'X'
                                      ? (st.opts.caps.unicode ? "\xE2\x98\x91 " : "[x] ")
                                      : (st.opts.caps.unicode ? "\xE2\x98\x90 " : "[ ] ");
                lc.pending = std::string(box);
            } else if (lc.ordered) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%d. ", lc.next_num++);
                lc.pending = buf;
            } else {
                lc.pending = st.opts.caps.unicode ? std::string("\xE2\x80\xA2 ")  // •
                                                  : std::string("* ");
            }
        }
        // Tight lists put text directly inside LI (no inner P), so
        // start an inline accumulator now. A nested block (or inner P)
        // will flush this on entry; otherwise leave_block(LI) flushes.
        st.inline_buf.clear();
        st.accumulating = true;
        break;
    }

    case MD_BLOCK_HR: {
        int w = std::min(st.opts.caps.columns, 80);
        std::string rule;
        if (st.opts.caps.unicode) {
            for (int i = 0; i < w; ++i)
                rule += "\xE2\x94\x80";  // ─
        } else {
            rule.assign(w, '-');
        }
        Style s;
        s.dim = true;
        st.out += st.block_prefix();
        st.out += st.sgr_open(s);
        st.out += rule;
        st.out += st.sgr_off();
        st.out += "\n";
        break;
    }

    case MD_BLOCK_H: {
        auto* d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        st.heading_level = d->level;
        st.accumulating = true;
        st.inline_buf.clear();
        Style s;
        s.bold = true;
        static const int colors[6] = {39, 33, 36, 35, 32, 90};
        s.fg_256 = colors[std::min<unsigned>(d->level - 1, 5)];
        st.push_style(s);
        for (unsigned i = 0; i < d->level; ++i)
            st.inline_buf += '#';
        st.inline_buf += ' ';
        break;
    }

    case MD_BLOCK_CODE: {
        auto* d = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
        st.in_code_block = true;
        st.code_lang.assign(d && d->lang.text ? d->lang.text : "",
                            d && d->lang.text ? d->lang.size : 0);
        st.inline_buf.clear();
        st.accumulating = true;
        break;
    }

    case MD_BLOCK_HTML: {
        st.inline_buf.clear();
        st.accumulating = true;
        Style s;
        s.dim = true;
        st.push_style(s);
        break;
    }

    case MD_BLOCK_P: {
        st.inline_buf.clear();
        st.accumulating = true;
        break;
    }

    case MD_BLOCK_TABLE: {
        st.in_table = true;
        st.table = TableState{};
        break;
    }
    case MD_BLOCK_THEAD:
        st.table.in_header = true;
        break;
    case MD_BLOCK_TBODY:
        st.table.in_header = false;
        break;
    case MD_BLOCK_TR:
        st.table.rows.push_back({});
        st.table.rows.back().is_header = st.table.in_header;
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        auto* d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
        if (d && st.table.aligns.size() <
                     (st.table.rows.empty() ? 0 : st.table.rows.front().cells.size()) + 1u) {
            st.table.aligns.push_back(d->align);
        } else if (d && st.table.aligns.empty()) {
            st.table.aligns.push_back(d->align);
        } else if (d) {
            size_t want = st.table.rows.back().cells.size() + 1;
            if (st.table.aligns.size() < want)
                st.table.aligns.push_back(d->align);
        }
        st.inline_buf.clear();
        st.accumulating = true;
        if (type == MD_BLOCK_TH) {
            Style s;
            s.bold = true;
            st.push_style(s);
        }
        break;
    }

    default:
        break;
    }
    return 0;
}

int leave_block_cb(MD_BLOCKTYPE type, void* /*detail*/, void* userdata) {
    RS& st = *static_cast<RS*>(userdata);
    switch (type) {
    case MD_BLOCK_DOC:
        break;

    case MD_BLOCK_QUOTE:
        st.quote_depth--;
        if (st.quote_depth == 0 && st.lists.empty())
            st.out += "\n";
        break;

    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        st.lists.pop_back();
        if (st.lists.empty() && st.quote_depth == 0)
            st.out += "\n";
        break;

    case MD_BLOCK_LI:
        // If tight-list text was accumulated directly inside LI, flush it.
        if (st.accumulating && !st.inline_buf.empty()) {
            emit_block(st, st.inline_buf);
        }
        st.accumulating = false;
        st.inline_buf.clear();
        drain_pending_images(st);
        break;

    case MD_BLOCK_HR:
        st.out += "\n";
        break;

    case MD_BLOCK_H: {
        st.pop_style();
        emit_block(st, st.inline_buf);
        st.out += "\n";
        st.accumulating = false;
        st.inline_buf.clear();
        st.heading_level = 0;
        drain_pending_images(st);
        break;
    }

    case MD_BLOCK_CODE: {
        st.accumulating = false;
        std::string prefix = st.block_prefix() + "    ";

        std::string_view text(st.inline_buf);
        // md4c emits a trailing '\n' on each line, including last.
        // Strip a single trailing '\n' to avoid an empty extra line.
        if (!text.empty() && text.back() == '\n')
            text.remove_suffix(1);

        Language lang = detect_language(st.code_lang, /*filename*/ {}, text);
        if (lang != Language::PlainText) {
            st.last_code_lang = st.code_lang;
            // Render through the highlighter — it handles per-line
            // prefixing and SGR sequences itself.
            ColorMode cm = st.color() ? st.opts.caps.color : ColorMode::None;
            highlight_to_stream(text, lang, cm, prefix, st.out);
        } else {
            Style s;
            s.fg_256 = 109;
            std::string open = st.sgr_open(s);
            std::string close = st.sgr_off();
            size_t start = 0;
            for (size_t i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == '\n') {
                    std::string_view line(text.data() + start, i - start);
                    st.out += prefix;
                    st.out += open;
                    st.out.append(line.data(), line.size());
                    st.out += close;
                    st.out += "\n";
                    start = i + 1;
                }
            }
        }
        if (st.lists.empty() && st.quote_depth == 0)
            st.out += "\n";
        st.inline_buf.clear();
        st.in_code_block = false;
        break;
    }

    case MD_BLOCK_HTML: {
        st.pop_style();
        // Strip trailing newline
        std::string_view text(st.inline_buf);
        if (!text.empty() && text.back() == '\n')
            text.remove_suffix(1);
        emit_block(st, text);
        if (st.lists.empty() && st.quote_depth == 0)
            st.out += "\n";
        st.accumulating = false;
        st.inline_buf.clear();
        break;
    }

    case MD_BLOCK_P: {
        emit_block(st, st.inline_buf);
        if (st.lists.empty() && st.quote_depth == 0)
            st.out += "\n";
        st.accumulating = false;
        st.inline_buf.clear();
        drain_pending_images(st);
        break;
    }

    case MD_BLOCK_TABLE:
        st.in_table = false;
        render_table(st);
        if (st.quote_depth == 0)
            st.out += "\n";
        break;
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
    case MD_BLOCK_TR:
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        if (type == MD_BLOCK_TH)
            st.pop_style();
        if (!st.table.rows.empty()) {
            st.table.rows.back().cells.push_back(st.inline_buf);
        }
        st.accumulating = false;
        st.inline_buf.clear();
        break;
    }

    default:
        break;
    }
    return 0;
}

int enter_span_cb(MD_SPANTYPE type, void* detail, void* userdata) {
    RS& st = *static_cast<RS*>(userdata);
    if (!st.accumulating)
        return 0;
    switch (type) {
    case MD_SPAN_EM: {
        Style s = st.top_style();
        s.italic = true;
        st.push_style(s);
        break;
    }
    case MD_SPAN_STRONG: {
        Style s = st.top_style();
        s.bold = true;
        st.push_style(s);
        break;
    }
    case MD_SPAN_U: {
        Style s = st.top_style();
        s.underline = true;
        st.push_style(s);
        break;
    }
    case MD_SPAN_DEL: {
        Style s = st.top_style();
        s.strike = true;
        st.push_style(s);
        break;
    }
    case MD_SPAN_CODE: {
        Style s = st.top_style();
        s.fg_256 = 109;
        s.reverse = false;
        st.push_style(s);
        // Start capturing the inline code text so we can tokenise it
        // when the span closes.
        st.in_inline_code = true;
        st.inline_code_buf.clear();
        break;
    }
    case MD_SPAN_A: {
        auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
        std::string url;
        if (d && d->href.text)
            url.assign(d->href.text, d->href.size);
        st.link_urls.push_back(url);
        if (st.opts.caps.hyperlinks && !st.opts.no_hyperlinks && !url.empty()) {
            st.inline_buf += hyperlink_open(url);
        }
        Style s = st.top_style();
        s.underline = true;
        s.fg_256 = 33;  // blue
        st.push_style(s);
        break;
    }
    case MD_SPAN_IMG: {
        auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
        st.in_image = true;
        st.pending_image_url.clear();
        if (d && d->src.text)
            st.pending_image_url.assign(d->src.text, d->src.size);
        Style s = st.top_style();
        s.fg_256 = 213;  // pink-ish
        st.push_style(s);
        st.inline_buf += "[";
        break;
    }
    case MD_SPAN_WIKILINK:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        break;
    }
    return 0;
}

int leave_span_cb(MD_SPANTYPE type, void* /*detail*/, void* userdata) {
    RS& st = *static_cast<RS*>(userdata);
    if (!st.accumulating)
        return 0;
    switch (type) {
    case MD_SPAN_EM:
    case MD_SPAN_STRONG:
    case MD_SPAN_U:
    case MD_SPAN_DEL:
        st.pop_style();
        break;
    case MD_SPAN_CODE: {
        // Finalise inline-code capture: highlight if we have a known
        // language context, else emit the captured text as-is (styled
        // by the active style stack).
        st.in_inline_code = false;
        std::string captured = std::move(st.inline_code_buf);
        st.inline_code_buf.clear();
        Language lang = detect_language(st.last_code_lang, {}, {});
        if (lang != Language::PlainText && !captured.empty()) {
            ColorMode cm = st.color() ? st.opts.caps.color : ColorMode::None;
            std::string highlighted;
            highlight_to_stream(captured, lang, cm, /*prefix*/ "", highlighted);
            // highlight_to_stream emits a trailing '\n'; inline code
            // must not carry one. Drop it (and any reset that landed
            // right before it).
            while (!highlighted.empty() && highlighted.back() == '\n') {
                highlighted.pop_back();
            }
            st.inline_buf += highlighted;
            // Re-open the surrounding style after the highlighter's
            // resets so subsequent inline text keeps its colour.
            if (st.color() && !st.style_stack.empty()) {
                st.inline_buf += st.sgr_open(st.style_stack.back());
            }
        } else {
            st.inline_buf += captured;
        }
        st.pop_style();
        break;
    }
    case MD_SPAN_A: {
        std::string url = st.link_urls.empty() ? std::string{} : st.link_urls.back();
        if (!st.link_urls.empty())
            st.link_urls.pop_back();
        st.pop_style();
        if (st.opts.caps.hyperlinks && !st.opts.no_hyperlinks && !url.empty()) {
            st.inline_buf += hyperlink_close();
        } else if (!url.empty()) {
            Style s;
            s.dim = true;
            st.inline_buf += " (";
            st.inline_buf += st.sgr_open(s);
            st.inline_buf += url;
            st.inline_buf += st.sgr_off();
            if (!st.style_stack.empty())
                st.inline_buf += st.sgr_open(st.style_stack.back());
            st.inline_buf += ")";
        }
        break;
    }
    case MD_SPAN_IMG: {
        st.pop_style();
        st.inline_buf += "]";

        std::string src = st.pending_image_url;
        bool remote = looks_remote(src);
        bool can_render = !src.empty() && !st.opts.no_images;
        if (remote) {
            // Only attempt remote rendering when explicitly allowed.
            can_render = can_render && st.opts.allow_web;
        }
        if (can_render) {
            RS::PendingImage pi;
            pi.path = remote ? src : resolve_local_path(src, st.opts.doc_dir);
            pi.remote = remote;
            st.pending_images.push_back(std::move(pi));
        } else if (!src.empty()) {
            Style s;
            s.dim = true;
            st.inline_buf += "(";
            st.inline_buf += st.sgr_open(s);
            st.inline_buf += src;
            st.inline_buf += st.sgr_off();
            if (!st.style_stack.empty())
                st.inline_buf += st.sgr_open(st.style_stack.back());
            st.inline_buf += ")";
        }
        st.in_image = false;
        st.pending_image_url.clear();
        break;
    }
    case MD_SPAN_WIKILINK:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        break;
    }
    return 0;
}

int text_cb(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
    RS& st = *static_cast<RS*>(userdata);
    if (!st.accumulating)
        return 0;
    // Redirect inline-code text into a side buffer so the close handler
    // can tokenise the full span.
    if (st.in_inline_code) {
        if (type == MD_TEXT_BR)
            st.inline_code_buf += '\n';
        else if (type == MD_TEXT_SOFTBR)
            st.inline_code_buf += ' ';
        else
            st.inline_code_buf.append(text, size);
        return 0;
    }
    switch (type) {
    case MD_TEXT_BR:
        st.inline_buf += '\n';
        break;
    case MD_TEXT_SOFTBR:
        st.inline_buf += ' ';
        break;
    case MD_TEXT_NULLCHAR:
        st.inline_buf += "\xEF\xBF\xBD";  // U+FFFD
        break;
    case MD_TEXT_ENTITY:
        // No resolution table — pass through literally. Authors who
        // want a specific character can write it as UTF-8.
        append_text(st, text, size);
        break;
    case MD_TEXT_CODE:
    case MD_TEXT_HTML:
    case MD_TEXT_NORMAL:
    case MD_TEXT_LATEXMATH:
    default:
        append_text(st, text, size);
        break;
    }
    return 0;
}

}  // namespace

bool render_markdown(std::string_view source, const RenderOptions& opts, std::string& out) {
    RS st;
    st.opts = opts;
    if (st.opts.plain) {
        st.opts.caps.color = ColorMode::None;
        st.opts.caps.hyperlinks = false;
        st.opts.caps.unicode = false;
        st.opts.no_hyperlinks = true;
        st.opts.no_images = true;
    }

    MD_PARSER p{};
    p.abi_version = 0;
    p.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS |
              MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_PERMISSIVEURLAUTOLINKS |
              MD_FLAG_PERMISSIVEWWWAUTOLINKS | MD_FLAG_PERMISSIVEEMAILAUTOLINKS |
              MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS;
    p.enter_block = enter_block_cb;
    p.leave_block = leave_block_cb;
    p.enter_span = enter_span_cb;
    p.leave_span = leave_span_cb;
    p.text = text_cb;
    p.debug_log = nullptr;
    p.syntax = nullptr;

    int rc = md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &p, &st);

    // Trim trailing blank lines to at most one
    while (st.out.size() >= 2 && st.out[st.out.size() - 1] == '\n' &&
           st.out[st.out.size() - 2] == '\n') {
        st.out.pop_back();
    }
    out = std::move(st.out);
    return rc == 0;
}

}  // namespace rcat
