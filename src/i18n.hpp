// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#pragma once

// Translation marker. Use `_("text")` for every user-facing string;
// xgettext scans for the macro and emits entries into po/rcat.pot.
// Marks strings that should be translated *later* (e.g. members of an
// array) without immediately translating them — call `_()` on the value
// at use site.
#define N_(s) (s)

#include <libintl.h>
#define _(s) gettext(s)

namespace rcat {

// Sets the program locale from the environment and points gettext at the
// installed (or build-tree) catalogs. Call once early in main(), before
// any `_()` lookups. Honors the env var `RCAT_LOCALEDIR` if set, so a
// run from the build tree can find catalogs without installation.
void init_i18n();

}  // namespace rcat
