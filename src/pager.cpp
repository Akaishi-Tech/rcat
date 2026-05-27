// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "pager.hpp"

#include "i18n.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

namespace rcat {

// ---------------------------------------------------------------------------
// ANSI-aware helpers
// ---------------------------------------------------------------------------

namespace {

// Advance past one ANSI escape sequence starting at s[i] (s[i] == ESC).
// Returns the new index. Handles CSI (ESC [ ... <final>) and OSC
// (ESC ] ... BEL  |  ESC ] ... ST).  Any other ESC <byte> swallows two.
std::size_t skip_escape(std::string_view s, std::size_t i) {
    if (i >= s.size() || s[i] != '\x1b')
        return i + 1;
    ++i;
    if (i >= s.size())
        return i;

    char c = s[i];
    if (c == '[') {
        ++i;
        while (i < s.size()) {
            unsigned char ch = static_cast<unsigned char>(s[i]);
            if (ch >= 0x40 && ch <= 0x7E) {
                ++i;
                break;
            }
            ++i;
        }
    } else if (c == ']') {
        ++i;
        while (i < s.size()) {
            if (s[i] == '\x07') {
                ++i;
                break;
            }
            if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '\\') {
                i += 2;
                break;
            }
            ++i;
        }
    } else {
        ++i;
    }
    return i;
}

inline char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty())
        return true;
    if (needle.size() > hay.size())
        return false;
    const std::size_t last = hay.size() - needle.size();
    for (std::size_t i = 0; i <= last; ++i) {
        std::size_t k = 0;
        for (; k < needle.size(); ++k) {
            if (ascii_lower(hay[i + k]) != ascii_lower(needle[k]))
                break;
        }
        if (k == needle.size())
            return true;
    }
    return false;
}

bool line_matches(std::string_view line, std::string_view needle, bool case_sensitive) {
    std::string plain = strip_ansi(line);
    if (case_sensitive)
        return plain.find(needle) != std::string::npos;
    return icontains(plain, needle);
}

}  // namespace

std::vector<std::string_view> split_visible_lines(std::string_view rendered) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i < rendered.size(); ++i) {
        if (rendered[i] == '\n') {
            out.emplace_back(rendered.data() + start, i - start);
            start = i + 1;
        }
    }
    if (start < rendered.size()) {
        out.emplace_back(rendered.data() + start, rendered.size() - start);
    }
    return out;
}

std::string strip_ansi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '\x1b') {
            i = skip_escape(s, i);
            continue;
        }
        out.push_back(s[i++]);
    }
    return out;
}

std::optional<std::size_t> find_next_match(const std::vector<std::string_view>& lines,
                                           std::string_view needle, std::size_t from,
                                           bool case_sensitive) {
    if (needle.empty())
        return std::nullopt;
    for (std::size_t i = from; i < lines.size(); ++i) {
        if (line_matches(lines[i], needle, case_sensitive))
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> find_prev_match(const std::vector<std::string_view>& lines,
                                           std::string_view needle, std::size_t from,
                                           bool case_sensitive) {
    if (needle.empty())
        return std::nullopt;
    if (lines.empty())
        return std::nullopt;
    std::size_t i = std::min(from, lines.size() - 1);
    while (true) {
        if (line_matches(lines[i], needle, case_sensitive))
            return i;
        if (i == 0)
            break;
        --i;
    }
    return std::nullopt;
}

std::size_t clamp_top(std::ptrdiff_t top, std::size_t viewport, std::size_t total) {
    if (total == 0)
        return 0;
    std::ptrdiff_t max_top =
        static_cast<std::ptrdiff_t>(total) - static_cast<std::ptrdiff_t>(viewport);
    if (max_top < 0)
        max_top = 0;
    if (top < 0)
        return 0;
    if (top > max_top)
        return static_cast<std::size_t>(max_top);
    return static_cast<std::size_t>(top);
}

// ===========================================================================
// Interactive pager
// ===========================================================================

namespace {

// SIGWINCH is delivered asynchronously; we just set a flag and check it on
// the next iteration of the input loop.
std::atomic<bool> g_resized{false};
void on_sigwinch(int) {
    g_resized.store(true, std::memory_order_relaxed);
}

// Query terminal size on `fd` (a tty). Falls back to 24x80.
struct Size {
    int rows;
    int cols;
};
Size query_size(int fd) {
    struct winsize ws{};
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        return {static_cast<int>(ws.ws_row), static_cast<int>(ws.ws_col)};
    }
    return {24, 80};
}

// RAII: open /dev/tty, switch to raw + alt screen, install SIGWINCH.
// On destruction restore everything.
class TtyGuard {
public:
    TtyGuard() : fd_(-1), have_old_(false), in_alt_(false) {}
    ~TtyGuard() { teardown(); }

    TtyGuard(const TtyGuard&) = delete;
    TtyGuard& operator=(const TtyGuard&) = delete;
    TtyGuard(TtyGuard&&) = delete;
    TtyGuard& operator=(TtyGuard&&) = delete;

    // Open /dev/tty and enter raw + alt screen. Returns true on success.
    bool setup() {
        fd_ = ::open("/dev/tty", O_RDWR | O_CLOEXEC);
        if (fd_ < 0)
            return false;
        if (!isatty(fd_)) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        if (tcgetattr(fd_, &old_) != 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        have_old_ = true;

        struct termios raw = old_;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
        raw.c_iflag &= ~(IXON | ICRNL | INPCK | ISTRIP | BRKINT);
        raw.c_oflag &= ~OPOST;
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(fd_, TCSAFLUSH, &raw) != 0)
            return false;

        // Enter alt screen, hide cursor.
        write_str("\x1b[?1049h\x1b[?25l");
        in_alt_ = true;

        struct sigaction sa{};
        sa.sa_handler = on_sigwinch;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGWINCH, &sa, &old_winch_);
        have_winch_ = true;

        return true;
    }

    int fd() const { return fd_; }

    void write_str(std::string_view s) {
        if (fd_ < 0)
            return;
        std::size_t off = 0;
        while (off < s.size()) {
            ssize_t n = ::write(fd_, s.data() + off, s.size() - off);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    void teardown() {
        if (in_alt_) {
            write_str("\x1b[0m\x1b[?25h\x1b[?1049l");
            in_alt_ = false;
        }
        if (have_old_ && fd_ >= 0) {
            tcsetattr(fd_, TCSAFLUSH, &old_);
            have_old_ = false;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        if (have_winch_) {
            sigaction(SIGWINCH, &old_winch_, nullptr);
            have_winch_ = false;
        }
    }

private:
    int fd_;
    struct termios old_;
    bool have_old_;
    bool in_alt_;
    struct sigaction old_winch_{};
    bool have_winch_{false};
};

// Read one byte from the tty. Returns -1 on error or signal.
int read_byte(int fd) {
    unsigned char b;
    for (;;) {
        ssize_t n = ::read(fd, &b, 1);
        if (n == 1)
            return b;
        if (n == 0)
            return -1;
        if (errno == EINTR) {
            if (g_resized.load(std::memory_order_relaxed))
                return -2;  // resize signal
            continue;
        }
        return -1;
    }
}

// Try to read a byte with timeout (ms). Returns -1 if nothing available.
int read_byte_timeout(int fd, int ms) {
    struct pollfd pfd{fd, POLLIN, 0};
    int pr = ::poll(&pfd, 1, ms);
    if (pr <= 0)
        return -1;
    unsigned char b;
    ssize_t n = ::read(fd, &b, 1);
    return n == 1 ? static_cast<int>(b) : -1;
}

// High-level key codes the dispatcher understands. We map raw bytes and
// escape sequences down to one of these so the dispatcher stays readable.
enum Key {
    K_NONE = -1,
    K_RESIZE = -2,
    K_EOF = -3,
    K_UP = 1000,
    K_DOWN,
    K_LEFT,
    K_RIGHT,
    K_HOME,
    K_END,
    K_PGUP,
    K_PGDN,
    K_ENTER,
    K_BACKSPACE,
    K_ESC,
};

// Read one logical key (a byte or a parsed escape sequence).
int read_key(int fd) {
    int b = read_byte(fd);
    if (b == -2)
        return K_RESIZE;
    if (b < 0)
        return K_EOF;

    if (b == '\r' || b == '\n')
        return K_ENTER;
    if (b == 0x7F || b == 0x08)
        return K_BACKSPACE;
    if (b != 0x1b)
        return b;

    // ESC: maybe an escape sequence. Peek for follow-up.
    int b1 = read_byte_timeout(fd, 50);
    if (b1 < 0)
        return K_ESC;
    if (b1 == '[' || b1 == 'O') {
        int b2 = read_byte_timeout(fd, 50);
        if (b2 < 0)
            return K_ESC;
        switch (b2) {
        case 'A':
            return K_UP;
        case 'B':
            return K_DOWN;
        case 'C':
            return K_RIGHT;
        case 'D':
            return K_LEFT;
        case 'H':
            return K_HOME;
        case 'F':
            return K_END;
        case '5':
        case '6':
        case '1':
        case '4':
        case '7':
        case '8': {
            // CSI <digits> ~  — keypad-style. Drain to '~' or final.
            int term = 0;
            while ((term = read_byte_timeout(fd, 50)) >= 0) {
                if (term == '~' || (term >= 0x40 && term <= 0x7E))
                    break;
            }
            if (b2 == '5')
                return K_PGUP;
            if (b2 == '6')
                return K_PGDN;
            if (b2 == '1' || b2 == '7')
                return K_HOME;
            if (b2 == '4' || b2 == '8')
                return K_END;
            return K_NONE;
        }
        default:
            return K_NONE;
        }
    }
    return K_ESC;
}

// Render the status bar text, no SGR. `cols` is the viewport width.
std::string status_text(const PagerOptions& opts, std::size_t top, std::size_t viewport,
                        std::size_t total, std::string_view message) {
    const std::string name = opts.filename.empty() ? std::string("(stdin)") : opts.filename;
    std::size_t shown_end = std::min(total, top + viewport);
    int pct = total == 0 ? 100 : static_cast<int>((shown_end * 100) / total);

    char buf[256];
    if (total == 0) {
        std::snprintf(buf, sizeof(buf), "%s  (empty)", name.c_str());
    } else if (shown_end >= total) {
        std::snprintf(buf, sizeof(buf), "%s  lines %zu-%zu/%zu  END", name.c_str(), top + 1,
                      shown_end, total);
    } else {
        std::snprintf(buf, sizeof(buf), "%s  lines %zu-%zu/%zu  %d%%", name.c_str(), top + 1,
                      shown_end, total, pct);
    }
    std::string out = buf;
    if (!message.empty()) {
        out += "   ";
        out.append(message);
    }
    out += "   ";
    out += _("[h] help  [q] quit");
    return out;
}

// Help text shown when the user presses `h`.
const char* help_text() {
    return _("Keys:\n"
             "  j  Down              scroll down one line\n"
             "  k  Up                scroll up one line\n"
             "  Space  f  PgDn       page down\n"
             "  b  PgUp              page up\n"
             "  d                    half page down\n"
             "  u                    half page up\n"
             "  g  Home              top of file\n"
             "  G  End               bottom of file\n"
             "  /pattern             search forward\n"
             "  ?pattern             search backward\n"
             "  n                    next match\n"
             "  N                    previous match\n"
             "  h                    this help\n"
             "  q                    quit\n"
             "\n"
             "Press any key to return.");
}

// Truncate a styled line so its visible width is at most `max_cells`.
// Bytes inside ANSI escape sequences are preserved; visible bytes after
// the cap are dropped. A trailing SGR reset is appended when we truncate.
std::string truncate_to_width(std::string_view line, int max_cells) {
    if (max_cells <= 0)
        return std::string{};
    std::string out;
    out.reserve(line.size());
    int width = 0;
    bool truncated = false;
    for (std::size_t i = 0; i < line.size();) {
        if (line[i] == '\x1b') {
            std::size_t end = skip_escape(line, i);
            out.append(line.data() + i, end - i);
            i = end;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(line[i]);
        int len = 1;
        int w = 1;
        if (c >= 0x80) {
            if ((c & 0xE0) == 0xC0)
                len = 2;
            else if ((c & 0xF0) == 0xE0)
                len = 3;
            else if ((c & 0xF8) == 0xF0)
                len = 4;
            // Multi-byte: assume width 1; the renderer already wraps to
            // cell width so this only matters when the terminal shrank.
        } else if (c < 0x20) {
            w = 0;
        }
        if (width + w > max_cells) {
            truncated = true;
            break;
        }
        if (i + static_cast<std::size_t>(len) > line.size()) {
            out.append(line.data() + i, line.size() - i);
            break;
        }
        out.append(line.data() + i, len);
        width += w;
        i += len;
    }
    if (truncated)
        out += "\x1b[0m";
    return out;
}

void paint_frame(TtyGuard& tty, const std::vector<std::string_view>& lines, std::size_t top,
                 int rows, int cols, const PagerOptions& opts, std::string_view message) {
    if (rows < 2)
        rows = 2;
    const int viewport_rows = rows - 1;

    std::string frame;
    frame.reserve(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols + 16));
    frame += "\x1b[H";  // cursor home
    for (int r = 0; r < viewport_rows; ++r) {
        std::size_t idx = top + static_cast<std::size_t>(r);
        frame += "\x1b[2K";  // clear line
        if (idx < lines.size()) {
            frame += truncate_to_width(lines[idx], cols);
            frame += "\x1b[0m";
        } else {
            // Past EOF — show a tilde like `less`/vim do.
            frame += "\x1b[2m~\x1b[0m";
        }
        frame += "\r\n";
    }
    // Status bar.
    frame += "\x1b[2K";
    if (!opts.no_color)
        frame += "\x1b[7m";
    std::string status =
        status_text(opts, top, static_cast<std::size_t>(viewport_rows), lines.size(), message);
    frame += truncate_to_width(status, cols);
    frame += "\x1b[0m";
    tty.write_str(frame);
}

void paint_full_screen(TtyGuard& tty, std::string_view text, int rows, int cols) {
    std::string frame = "\x1b[H\x1b[2J";
    // Emit `text` line by line so we can pad with clear-line and not
    // depend on the terminal honoring trailing whitespace.
    auto lines = split_visible_lines(text);
    int painted = 0;
    for (auto& l : lines) {
        if (painted >= rows - 1)
            break;
        frame += "\x1b[2K";
        frame += truncate_to_width(l, cols);
        frame += "\r\n";
        ++painted;
    }
    while (painted < rows - 1) {
        frame += "\x1b[2K\r\n";
        ++painted;
    }
    frame += "\x1b[2K\x1b[7m";
    frame += truncate_to_width(_("[Press any key to return]"), cols);
    frame += "\x1b[0m";
    tty.write_str(frame);
}

// Read a single line of input at the status bar. Returns std::nullopt if
// the user hits ESC; otherwise the entered string (may be empty on Enter).
std::optional<std::string> prompt(TtyGuard& tty, int rows, int cols, std::string_view leader) {
    std::string buf;
    auto repaint = [&] {
        std::string s;
        char move[32];
        std::snprintf(move, sizeof(move), "\x1b[%d;1H", rows);
        s += move;
        s += "\x1b[2K";
        s.append(leader);
        s.append(buf);
        // Try to keep visible width within cols.
        std::string truncated = truncate_to_width(s, cols);
        tty.write_str(truncated);
    };
    repaint();
    for (;;) {
        int k = read_key(tty.fd());
        if (k == K_RESIZE) {
            repaint();
            continue;
        }
        if (k == K_ENTER)
            return buf;
        if (k == K_ESC)
            return std::nullopt;
        if (k == K_BACKSPACE) {
            if (!buf.empty())
                buf.pop_back();
            repaint();
            continue;
        }
        if (k >= 0x20 && k < 0x7F) {
            buf.push_back(static_cast<char>(k));
            repaint();
        }
    }
}

}  // namespace

int run_pager(std::string rendered, const PagerOptions& opts) {
    TtyGuard tty;
    if (!tty.setup())
        return 1;

    Size sz = query_size(tty.fd());

    // If the caller gave us a rerender callback, prime the buffer for the
    // current terminal width so the initial frame is wrapped correctly.
    if (opts.rerender)
        rendered = opts.rerender(sz.cols);
    auto lines = split_visible_lines(rendered);

    std::size_t top = 0;
    std::string last_query;
    bool last_query_forward = true;
    std::string message;

    auto refresh_layout = [&] {
        sz = query_size(tty.fd());
        if (opts.rerender) {
            rendered = opts.rerender(sz.cols);
            lines = split_visible_lines(rendered);
        }
        top = clamp_top(static_cast<std::ptrdiff_t>(top),
                        static_cast<std::size_t>(std::max(1, sz.rows - 1)), lines.size());
    };

    for (;;) {
        if (g_resized.exchange(false, std::memory_order_relaxed)) {
            refresh_layout();
        }
        paint_frame(tty, lines, top, sz.rows, sz.cols, opts, message);
        message.clear();

        int k = read_key(tty.fd());
        const std::size_t viewport = static_cast<std::size_t>(std::max(1, sz.rows - 1));

        if (k == K_RESIZE) {
            refresh_layout();
            continue;
        }
        if (k == K_EOF)
            return 0;
        if (k == K_NONE) {
            continue;
        }

        auto move_to = [&](std::ptrdiff_t new_top) {
            top = clamp_top(new_top, viewport, lines.size());
        };

        switch (k) {
        case 'q':
        case 'Q':
        case 0x03:  // Ctrl-C
            return 0;

        case 'j':
        case K_DOWN:
            move_to(static_cast<std::ptrdiff_t>(top) + 1);
            break;
        case 'k':
        case K_UP:
            move_to(static_cast<std::ptrdiff_t>(top) - 1);
            break;
        case ' ':
        case 'f':
        case K_PGDN:
            move_to(static_cast<std::ptrdiff_t>(top) + static_cast<std::ptrdiff_t>(viewport));
            break;
        case 'b':
        case K_PGUP:
            move_to(static_cast<std::ptrdiff_t>(top) - static_cast<std::ptrdiff_t>(viewport));
            break;
        case 'd':
            move_to(static_cast<std::ptrdiff_t>(top) + static_cast<std::ptrdiff_t>(viewport / 2));
            break;
        case 'u':
            move_to(static_cast<std::ptrdiff_t>(top) - static_cast<std::ptrdiff_t>(viewport / 2));
            break;
        case 'g':
        case K_HOME:
            top = 0;
            break;
        case 'G':
        case K_END:
            move_to(static_cast<std::ptrdiff_t>(lines.size()));
            break;

        case '/':
        case '?': {
            bool forward = (k == '/');
            auto q = prompt(tty, sz.rows, sz.cols, forward ? "/" : "?");
            if (!q || q->empty()) {
                message = _("Search cancelled");
                break;
            }
            last_query = *q;
            last_query_forward = forward;
            auto m = forward ? find_next_match(lines, last_query, top, false)
                             : find_prev_match(lines, last_query, top == 0 ? 0 : top - 1, false);
            if (m) {
                top = clamp_top(static_cast<std::ptrdiff_t>(*m), viewport, lines.size());
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s %zu", _("match at line"), *m + 1);
                message = buf;
            } else {
                message = _("Pattern not found");
            }
            break;
        }

        case 'n': {
            if (last_query.empty()) {
                message = _("No previous search");
                break;
            }
            std::size_t start = last_query_forward ? top + 1 : top;
            auto m = last_query_forward
                         ? find_next_match(lines, last_query, start, false)
                         : find_prev_match(lines, last_query, start == 0 ? 0 : start - 1, false);
            if (m)
                top = clamp_top(static_cast<std::ptrdiff_t>(*m), viewport, lines.size());
            else
                message = _("Pattern not found");
            break;
        }
        case 'N': {
            if (last_query.empty()) {
                message = _("No previous search");
                break;
            }
            std::size_t start = last_query_forward ? (top == 0 ? 0 : top - 1) : top + 1;
            auto m = last_query_forward ? find_prev_match(lines, last_query, start, false)
                                        : find_next_match(lines, last_query, start, false);
            if (m)
                top = clamp_top(static_cast<std::ptrdiff_t>(*m), viewport, lines.size());
            else
                message = _("Pattern not found");
            break;
        }

        case 'h':
        case 'H':
            paint_full_screen(tty, help_text(), sz.rows, sz.cols);
            read_key(tty.fd());
            break;

        default:
            break;
        }
    }
}

}  // namespace rcat
