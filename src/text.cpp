#define STB_TRUETYPE_IMPLEMENTATION
#include "text.h"

#include "font.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

/* pow(cov/255, 0.78) as a lookup table — light-on-dark text looks thin with
   plain coverage, a mild gamma makes antialiased edges read as sharp. */
const uint8_t COVERAGE_LUT[256] = {
#define COV(n) uint8_t(std::lround(std::pow(n / 255.0, 0.78) * 255.0))
    COV(0), COV(1), COV(2), COV(3), COV(4), COV(5), COV(6), COV(7), COV(8), COV(9), COV(10),
    COV(11), COV(12), COV(13), COV(14), COV(15), COV(16), COV(17), COV(18), COV(19), COV(20),
    COV(21), COV(22), COV(23), COV(24), COV(25), COV(26), COV(27), COV(28), COV(29), COV(30),
    COV(31), COV(32), COV(33), COV(34), COV(35), COV(36), COV(37), COV(38), COV(39), COV(40),
    COV(41), COV(42), COV(43), COV(44), COV(45), COV(46), COV(47), COV(48), COV(49), COV(50),
    COV(51), COV(52), COV(53), COV(54), COV(55), COV(56), COV(57), COV(58), COV(59), COV(60),
    COV(61), COV(62), COV(63), COV(64), COV(65), COV(66), COV(67), COV(68), COV(69), COV(70),
    COV(71), COV(72), COV(73), COV(74), COV(75), COV(76), COV(77), COV(78), COV(79), COV(80),
    COV(81), COV(82), COV(83), COV(84), COV(85), COV(86), COV(87), COV(88), COV(89), COV(90),
    COV(91), COV(92), COV(93), COV(94), COV(95), COV(96), COV(97), COV(98), COV(99), COV(100),
    COV(101), COV(102), COV(103), COV(104), COV(105), COV(106), COV(107), COV(108), COV(109),
    COV(110), COV(111), COV(112), COV(113), COV(114), COV(115), COV(116), COV(117), COV(118),
    COV(119), COV(120), COV(121), COV(122), COV(123), COV(124), COV(125), COV(126), COV(127),
    COV(128), COV(129), COV(130), COV(131), COV(132), COV(133), COV(134), COV(135), COV(136),
    COV(137), COV(138), COV(139), COV(140), COV(141), COV(142), COV(143), COV(144), COV(145),
    COV(146), COV(147), COV(148), COV(149), COV(150), COV(151), COV(152), COV(153), COV(154),
    COV(155), COV(156), COV(157), COV(158), COV(159), COV(160), COV(161), COV(162), COV(163),
    COV(164), COV(165), COV(166), COV(167), COV(168), COV(169), COV(170), COV(171), COV(172),
    COV(173), COV(174), COV(175), COV(176), COV(177), COV(178), COV(179), COV(180), COV(181),
    COV(182), COV(183), COV(184), COV(185), COV(186), COV(187), COV(188), COV(189), COV(190),
    COV(191), COV(192), COV(193), COV(194), COV(195), COV(196), COV(197), COV(198), COV(199),
    COV(200), COV(201), COV(202), COV(203), COV(204), COV(205), COV(206), COV(207), COV(208),
    COV(209), COV(210), COV(211), COV(212), COV(213), COV(214), COV(215), COV(216), COV(217),
    COV(218), COV(219), COV(220), COV(221), COV(222), COV(223), COV(224), COV(225), COV(226),
    COV(227), COV(228), COV(229), COV(230), COV(231), COV(232), COV(233), COV(234), COV(235),
    COV(236), COV(237), COV(238), COV(239), COV(240), COV(241), COV(242), COV(243), COV(244),
    COV(245), COV(246), COV(247), COV(248), COV(249), COV(250), COV(251), COV(252), COV(253),
    COV(254), COV(255),
#undef COV
};

/* MiniLyrics draws lyrics in a bold Arial-class face with a black outline;
   these are the closest matches on each platform, in order. */
const char *FONT_CANDIDATES[] = {
    // Windows (the original look)
    "C:/Windows/Fonts/arialbd.ttf",
    "C:/Windows/Fonts/tahomabd.ttf",
    // macOS
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/Library/Fonts/Arial Bold.ttf",
    // Linux — Liberation Sans Bold / Arimo Bold are metric clones of Arial
    "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/arimo/Arimo-Bold.ttf",
    "/usr/share/fonts/TTF/Arimo-Bold.ttf",
    "/usr/share/fonts/TTF/arialbd.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
    "/usr/share/fonts/TTF/tahoma.ttf",
    // Helvetica / DejaVu / Noto as last resort
    "/usr/share/fonts/gsfonts/NimbusSans-Bold.otf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};

/* Faces that fill in scripts the primary font usually does not cover. */
const char *FALLBACK_CANDIDATES[] = {
    // Bold first: it should match the weight of the Arial-class primary face.
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Medium.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
    "/usr/share/fonts/truetype/noto/NotoSansThai-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansArabic-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};

uint32_t nextCodepoint(std::string_view s, size_t &i)
{
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        ++i;
        return c;
    }
    auto cont = [&](size_t k) { return i + k < s.size() && ((unsigned char)s[i + k] & 0xC0) == 0x80; };
    if ((c & 0xE0) == 0xC0 && cont(1)) {
        const uint32_t cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && cont(1) && cont(2)) {
        const uint32_t cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) |
                            ((unsigned char)s[i + 2] & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) {
        const uint32_t cp = ((c & 0x07u) << 18) | (((unsigned char)s[i + 1] & 0x3Fu) << 12) |
                            (((unsigned char)s[i + 2] & 0x3Fu) << 6) | ((unsigned char)s[i + 3] & 0x3Fu);
        i += 4;
        return cp;
    }
    ++i;
    return '?';
}

struct Packed {
    int x = 0, y = 0, w = 0, h = 0;
};

} // namespace

bool UiText::addFace(const std::string &path, int fontIndex) const
{
    const std::string data = util::readFile(path);
    if (data.size() < 1024)
        return false;

    Face face;
    face.data.assign(data.begin(), data.end());

    for (int index = fontIndex;; ++index) {
        const int offset = stbtt_GetFontOffsetForIndex(face.data.data(), index);
        if (offset < 0)
            break;
        stbtt_fontinfo info{};
        if (stbtt_InitFont(&info, face.data.data(), offset)) {
            face.info = info;
            face.fontIndex = index;
            m_faces.push_back(std::move(face));
            return true;
        }
    }
    return false;
}

/* Fonts for scripts the primary face does not cover are opened only when a
   lyric actually needs them — Noto Sans CJK alone is 20 MB, no point loading
   it for an English song. */
bool UiText::loadNextFallback() const
{
    constexpr size_t n = sizeof(FALLBACK_CANDIDATES) / sizeof(FALLBACK_CANDIDATES[0]);
    while (size_t(m_nextFallback) < n) {
        const char *path = FALLBACK_CANDIDATES[size_t(m_nextFallback)];
        ++m_nextFallback;
        if (addFace(path))
            return true;
    }
    return false;
}

bool UiText::load()
{
    m_faces.clear();
    m_atlases.clear();
    m_ok = false;

    if (const char *env = std::getenv("LINYRICS_FONT"); env && *env) {
        if (!addFace(env))
            std::fprintf(stderr, "linyrics: could not load font '%s'\n", env);
    }

    if (m_faces.empty()) {
        for (const char *path : FONT_CANDIDATES) {
            if (addFace(path))
                break;
        }
    }

    if (m_faces.empty()) {
        // nothing Arial-like installed: any decent user font beats giving up
        if (const char *home = std::getenv("HOME"); home && *home) {
            static const char *USER_FONTS[] = {
                "/.local/share/fonts/Figtree/Figtree-SemiBold.ttf",
                "/.local/share/fonts/Figtree/Figtree-Medium.ttf",
                "/.local/share/fonts/inter/Inter-SemiBold.ttf",
                "/.local/share/fonts/Inter/Inter-SemiBold.ttf",
                "/.local/share/fonts/Poppins/Poppins-SemiBold.ttf",
                "/.local/share/fonts/PlusJakartaSans/PlusJakartaSans-SemiBold.ttf",
            };
            for (const char *rel : USER_FONTS) {
                if (addFace(std::string(home) + rel))
                    break;
            }
        }
    }

    if (m_faces.empty())
        return false;

    m_path = "primary";
    m_ok = true;
    m_nextFallback = 0;
    return true;
}

UiText::Atlas *UiText::atlasFor(int px) const
{
    if (!m_ok || px <= 0 || m_faces.empty())
        return nullptr;
    auto it = m_atlases.find(px);
    if (it != m_atlases.end())
        return &it->second;

    Atlas atlas;
    atlas.px = px;
    atlas.width = std::max(512, px * 2 + 16);
    const float scale = stbtt_ScaleForPixelHeight(&m_faces.front().info, float(px));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&m_faces.front().info, &ascent, &descent, &lineGap);
    atlas.ascent = int(std::ceil(ascent * scale));
    atlas.lineHeight = int(std::ceil((ascent - descent + lineGap) * scale));
    auto res = m_atlases.emplace(px, std::move(atlas));
    return &res.first->second;
}

UiText::Glyph UiText::glyphFor(Atlas &atlas, uint32_t cp) const
{
    auto it = atlas.glyphs.find(cp);
    if (it != atlas.glyphs.end())
        return it->second;

    const int pxDev = atlas.px;

    for (;;) {
        const Face *facePtr = nullptr;
        for (const Face &f : m_faces) {
            if (stbtt_FindGlyphIndex(&f.info, int(cp)) != 0) {
                facePtr = &f;
                break;
            }
        }
        if (!facePtr) {
            if (loadNextFallback())
                continue;
            break;
        }
        const Face &face = *facePtr;

        const float scale = stbtt_ScaleForPixelHeight(&face.info, float(pxDev));
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&face.info, int(cp), &adv, &lsb);
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&face.info, int(cp), scale, scale, &x0, &y0, &x1, &y1);

        Glyph g;
        g.xadv = int(std::lround(adv * scale));
        g.w = x1 - x0;
        g.h = y1 - y0;
        g.xoff = x0;
        g.yoff = y0;

        if (g.w > 0 && g.h > 0) {
            if (g.w + 1 > atlas.width)
                g.w = atlas.width - 1;
            if (atlas.penX + g.w + 1 > atlas.width) {
                atlas.penX = 0;
                atlas.penY += atlas.rowHeight + 1;
                atlas.rowHeight = 0;
            }
            const int needH = atlas.penY + g.h + 1;
            if (needH > atlas.height) {
                atlas.height = needH + 32;
                atlas.mask.resize(size_t(atlas.width) * size_t(atlas.height), 0);
            }
            std::vector<uint8_t> bmp(size_t(g.w) * size_t(g.h), 0);
            stbtt_MakeCodepointBitmap(&face.info, bmp.data(), g.w, g.h, g.w, scale, scale, int(cp));
            for (uint8_t &cov : bmp)
                if (cov)
                    cov = COVERAGE_LUT[cov];

            g.x = atlas.penX;
            g.y = atlas.penY;
            for (int row = 0; row < g.h; ++row) {
                const uint8_t *src = bmp.data() + size_t(row) * size_t(g.w);
                uint8_t *dst = atlas.mask.data() + size_t(g.y + row) * size_t(atlas.width) +
                               size_t(g.x);
                std::copy(src, src + g.w, dst);
            }
            atlas.penX += g.w + 1;
            atlas.rowHeight = std::max(atlas.rowHeight, g.h);

            // Dilated silhouette for the outline pass: union of the glyph mask
            // shifted in 8 directions, padded so it can be drawn around the
            // text without recomputing anything per frame.
            const int pad = std::max(1, (pxDev + 8) / 16);
            const int ow = g.w + 2 * pad;
            const int oh = g.h + 2 * pad;
            std::vector<uint8_t> dil(size_t(ow) * size_t(oh), 0);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int row = 0; row < g.h; ++row) {
                        const uint8_t *src = bmp.data() + size_t(row) * size_t(g.w);
                        uint8_t *dst = dil.data() + size_t(row + pad + dy) * size_t(ow) +
                                       size_t(pad + dx);
                        for (int col = 0; col < g.w; ++col)
                            dst[col] = std::max(dst[col], src[col]);
                    }
                }
            }

            if (atlas.penX + ow + 1 > atlas.width) {
                atlas.penX = 0;
                atlas.penY += atlas.rowHeight + 1;
                atlas.rowHeight = 0;
            }
            const int needH2 = atlas.penY + oh + 1;
            if (needH2 > atlas.height) {
                atlas.height = needH2 + 32;
                atlas.mask.resize(size_t(atlas.width) * size_t(atlas.height), 0);
            }
            g.ox = atlas.penX;
            g.oy = atlas.penY;
            g.ow = ow;
            g.oh = oh;
            g.pad = pad;
            for (int row = 0; row < oh; ++row) {
                const uint8_t *src = dil.data() + size_t(row) * size_t(ow);
                uint8_t *dst = atlas.mask.data() + size_t(g.oy + row) * size_t(atlas.width) + size_t(g.ox);
                std::copy(src, src + ow, dst);
            }
            atlas.penX += ow + 1;
            atlas.rowHeight = std::max(atlas.rowHeight, oh);
        }

        atlas.glyphs[cp] = g;
        return g;
    }

    // nothing in any face: leave a blank advance so spacing stays sane
    Glyph g;
    g.w = 0;
    g.h = 0;
    g.xadv = pxDev / 2;
    atlas.glyphs[cp] = g;
    return g;
}

int UiText::measure(std::string_view text, int px) const
{
    if (!m_ok) {
        return pf::measure(text, std::max(1, px / 7), 1);
    }
    const int pxDev = std::max(6, int(std::lround(double(px) * double(m_pxScale))));
    Atlas *atlas = atlasFor(pxDev);
    if (!atlas)
        return pf::measure(text, 1, 1);
    int width = 0;
    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        width += glyphFor(*atlas, cp).xadv;
    }
    return int(std::lround(double(width) / double(m_pxScale)));
}

int UiText::lineHeight(int px) const
{
    const int pxDev = std::max(6, int(std::lround(double(px) * double(m_pxScale))));
    const Atlas *atlas = atlasFor(pxDev);
    if (!atlas)
        return pf::CH_H * std::max(1, px / 7);
    return std::max(1, int(std::lround(double(atlas->lineHeight) / double(m_pxScale))));
}

void UiText::draw(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const
{
    // rasterise at the device resolution so fractional HiDPI stays sharp
    const float s = cv.scaleX();
    const int pxDev = std::max(6, int(std::lround(double(px) * double(s))));
    Atlas *atlas = atlasFor(pxDev);
    if (!atlas) {
        pf::draw(cv, text, x, y, std::max(1, px / 7), color);
        return;
    }
    const int baseline = cv.toDeviceY(float(y)) + atlas->ascent;
    int penX = cv.toDeviceX(float(x));
    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        const Glyph g = glyphFor(*atlas, cp);
        if (g.w > 0 && g.h > 0) {
            const uint8_t *mask =
                atlas->mask.data() + size_t(g.y) * size_t(atlas->width) + size_t(g.x);
            cv.blendMask(penX + g.xoff, baseline + g.yoff, g.w, g.h, mask, atlas->width, color);
        }
        penX += g.xadv;
    }
}

void UiText::drawOutline(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const
{
    const float s = cv.scaleX();
    const int pxDev = std::max(6, int(std::lround(double(px) * double(s))));
    Atlas *atlas = atlasFor(pxDev);
    if (!atlas) {
        pf::draw(cv, text, x, y, std::max(1, px / 7), color);
        return;
    }
    const int baseline = cv.toDeviceY(float(y)) + atlas->ascent;
    int penX = cv.toDeviceX(float(x));
    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        const Glyph g = glyphFor(*atlas, cp);
        if (g.ow > 0 && g.oh > 0) {
            const uint8_t *mask =
                atlas->mask.data() + size_t(g.oy) * size_t(atlas->width) + size_t(g.ox);
            cv.blendMask(penX + g.xoff - g.pad, baseline + g.yoff - g.pad, g.ow, g.oh, mask,
                         atlas->width, color);
        }
        penX += g.xadv;
    }
}

void UiText::drawIn(Canvas &cv, std::string_view text, Rect box, int align, int px, Color color) const
{
    const int w = measure(text, px);
    int x = box.x;
    if (align == 1)
        x = box.x + (box.w - w) / 2;
    else if (align == 2)
        x = box.right() - w;
    const int y = box.y + (box.h - lineHeight(px)) / 2;
    draw(cv, text, x, y, px, color);
}
