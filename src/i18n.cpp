// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "i18n.hpp"

#include <clocale>
#include <cstdlib>

#ifdef RCAT_HAVE_GETTEXT
#include <libintl.h>
#endif

#ifndef RCAT_LOCALEDIR
#define RCAT_LOCALEDIR "/usr/share/locale"
#endif

namespace rcat {

void init_i18n() {
    // LC_ALL "" picks the user's full locale (LC_MESSAGES, LC_CTYPE for
    // wcwidth, LC_NUMERIC, etc.) from the environment. This supersedes
    // the LC_CTYPE-only setlocale we'd otherwise need for wcwidth.
    std::setlocale(LC_ALL, "");

#ifdef RCAT_HAVE_GETTEXT
    const char* dir = std::getenv("RCAT_LOCALEDIR");
    if (!dir || !*dir)
        dir = RCAT_LOCALEDIR;
    bindtextdomain("rcat", dir);
    bind_textdomain_codeset("rcat", "UTF-8");
    textdomain("rcat");
#endif
}

}  // namespace rcat
