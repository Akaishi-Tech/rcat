// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Akaishi Tech

#include "image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <chafa.h>

#ifdef RCAT_HAVE_CURL
#  include <curl/curl.h>
#endif

#include <cstring>
#include <string>

namespace rcat {

bool image_rendering_available() { return true; }

#ifdef RCAT_HAVE_CURL
bool web_fetch_available() { return true; }
#else
bool web_fetch_available() { return false; }
#endif

namespace {

ChafaCanvasMode canvas_mode_for(ColorMode c) {
    switch (c) {
        case ColorMode::TrueColor: return CHAFA_CANVAS_MODE_TRUECOLOR;
        case ColorMode::Ansi256:   return CHAFA_CANVAS_MODE_INDEXED_240;
        case ColorMode::Ansi16:    return CHAFA_CANVAS_MODE_INDEXED_16;
        case ColorMode::None:      return CHAFA_CANVAS_MODE_FGBG;
    }
    return CHAFA_CANVAS_MODE_TRUECOLOR;
}

ImageRenderResult render_pixels(unsigned char* pixels,
                                int w, int h,
                                int max_width_cells,
                                int max_height_cells,
                                ColorMode color) {
    ImageRenderResult r;
    if (max_width_cells  < 1) max_width_cells  = 1;
    if (max_height_cells < 1) max_height_cells = 1;

    ChafaSymbolMap* sm = chafa_symbol_map_new();
    chafa_symbol_map_add_by_tags(sm, CHAFA_SYMBOL_TAG_ALL);

    ChafaCanvasConfig* cfg = chafa_canvas_config_new();
    chafa_canvas_config_set_geometry(cfg, max_width_cells, max_height_cells);
    chafa_canvas_config_set_canvas_mode(cfg, canvas_mode_for(color));
    chafa_canvas_config_set_pixel_mode(cfg, CHAFA_PIXEL_MODE_SYMBOLS);
    chafa_canvas_config_set_symbol_map(cfg, sm);

    ChafaCanvas* canvas = chafa_canvas_new(cfg);
    chafa_canvas_draw_all_pixels(canvas,
                                 CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                 pixels, w, h, w * 4);

    GString* gs = chafa_canvas_print(canvas, nullptr);
    if (gs) {
        r.output.assign(gs->str, gs->len);
        g_string_free(gs, TRUE);
    }

    gint cw = 0, ch = 0;
    chafa_canvas_config_get_geometry(cfg, &cw, &ch);
    r.width_cells  = cw;
    r.height_cells = ch;

    chafa_canvas_unref(canvas);
    chafa_canvas_config_unref(cfg);
    chafa_symbol_map_unref(sm);

    r.success = true;
    return r;
}

} // namespace

ImageRenderResult render_image_file(std::string_view path,
                                    int max_width_cells,
                                    int max_height_cells,
                                    ColorMode color) {
    ImageRenderResult r;
    std::string path_str(path);
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path_str.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        r.error = reason ? reason : "stbi_load failed";
        return r;
    }
    r = render_pixels(pixels, w, h, max_width_cells, max_height_cells, color);
    stbi_image_free(pixels);
    return r;
}

ImageRenderResult render_image_bytes(const unsigned char* bytes,
                                     size_t bytes_len,
                                     int max_width_cells,
                                     int max_height_cells,
                                     ColorMode color) {
    ImageRenderResult r;
    if (!bytes || bytes_len == 0) {
        r.error = "empty image buffer";
        return r;
    }
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        bytes, static_cast<int>(bytes_len), &w, &h, &channels, 4);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        r.error = reason ? reason : "stbi_load_from_memory failed";
        return r;
    }
    r = render_pixels(pixels, w, h, max_width_cells, max_height_cells, color);
    stbi_image_free(pixels);
    return r;
}

#ifdef RCAT_HAVE_CURL

namespace {

struct FetchCtx {
    std::string* out;
    size_t max_bytes;
    bool   exceeded;
};

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<FetchCtx*>(userdata);
    size_t n = size * nmemb;
    if (ctx->out->size() + n > ctx->max_bytes) {
        ctx->exceeded = true;
        return 0; // abort transfer
    }
    ctx->out->append(ptr, n);
    return n;
}

} // namespace

WebFetchResult fetch_url(std::string_view url,
                         int timeout_seconds,
                         size_t max_bytes) {
    WebFetchResult r;
    std::string url_str(url);

    static bool inited = []() {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }();
    if (!inited) {
        r.error = "curl_global_init failed";
        return r;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        r.error = "curl_easy_init failed";
        return r;
    }

    FetchCtx ctx{&r.bytes, max_bytes, false};

    curl_easy_setopt(curl, CURLOPT_URL,             url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,  1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS,       5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,         (long)timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,  (long)timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,        1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR,     1L); // 4xx/5xx -> error
    curl_easy_setopt(curl, CURLOPT_USERAGENT,       "rcat/0.1");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,   curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,       &ctx);
    // Restrict to http/https only -- prevents file://, gopher://, etc. via redirect.
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR,       "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        r.success = true;
    } else if (ctx.exceeded) {
        r.error = "response exceeded max_bytes";
        r.bytes.clear();
    } else {
        r.error = curl_easy_strerror(rc);
        r.bytes.clear();
    }

    curl_easy_cleanup(curl);
    return r;
}

#else // !RCAT_HAVE_CURL

WebFetchResult fetch_url(std::string_view, int, size_t) {
    WebFetchResult r;
    r.error = "rcat built without libcurl support";
    return r;
}

#endif

} // namespace rcat
