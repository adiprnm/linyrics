/* ------------------------------------------------------------------
   Linyrics — text rendering.

   TrueType rasterisation through stb_truetype with a lazily grown glyph
   atlas: only the glyphs a lyric sheet actually uses are rasterised, so
   CJK / Cyrillic / Greek lyrics are covered without bundling a font or
   paying for thousands of unused glyphs at startup.

   The primary font is the system UI font; Noto Sans CJK (when installed)
   is used as a fallback for scripts the primary face does not cover.
   No font file is bundled with the app.
   ------------------------------------------------------------------ */
#pragma once

#include "gfx.h"

#include "third_party/stb_truetype.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class UiText {
public:
    /* Tries $LINYRICS_FONT, then per-user fonts, then the usual system paths. */
    bool load();

    bool ok() const { return m_ok; }
    const std::string &fontPath() const { return m_path; }

    /* Logical -> device pixel factor of the canvas (fractional HiDPI). */
    void setPixelScale(float s)
    {
        m_pxScale = (s > 0.05f) ? s : 1.f;
        m_atlases.clear();
    }

    /* Antialiased text; falls back to the pixel font when no TTF is loaded.
       `y` is the top of the line box (baseline is y + ascent). */
    void draw(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const;
    /* Draws the glyph silhouette grown by `px / 22` device pixels — one pass,
       because the dilated mask is rasterised once per glyph and cached. Used
       for the MiniLyrics-style black outline. */
    void drawOutline(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const;
    /* Draws inside a box: vertically centred, horizontally aligned (0/1/2). */
    void drawIn(Canvas &cv, std::string_view text, Rect box, int align, int px, Color color) const;
    /* Width in pixels, and a line-box height for the given size. */
    int measure(std::string_view text, int px) const;
    int lineHeight(int px) const;

private:
    struct Face {
        std::vector<uint8_t> data;
        stbtt_fontinfo info{};
        int fontIndex = 0;
    };

    struct Glyph {
        int x = 0, y = 0, w = 0, h = 0;
        int xoff = 0, yoff = 0, xadv = 0;
        // dilated copy (outline), already padded by `pad`
        int ox = 0, oy = 0, ow = 0, oh = 0, pad = 0;
    };

    struct Atlas {
        int px = 0;
        int width = 512;
        int height = 0;
        int lineHeight = 0;
        int ascent = 0;
        int penX = 0, penY = 0, rowHeight = 0;
        std::vector<uint8_t> mask;
        std::map<uint32_t, Glyph> glyphs;
    };

    Atlas *atlasFor(int px) const;
    Glyph glyphFor(Atlas &atlas, uint32_t cp) const;
    bool addFace(const std::string &path, int fontIndex = 0) const;
    bool loadNextFallback() const;

    float m_pxScale = 1.f;
    bool m_ok = false;
    std::string m_path;
    mutable std::vector<Face> m_faces;
    mutable int m_nextFallback = 0;
    mutable std::map<int, Atlas> m_atlases;
};
