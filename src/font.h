/* ------------------------------------------------------------------
   Linamp — 5x7 pixel font (port of qml/PixelFont.js).
   Text is drawn as rectangles, so no font engine / freetype needed.
   ------------------------------------------------------------------ */
#pragma once

#include "gfx.h"

#include <string>
#include <string_view>

namespace pf {

constexpr int CH_W = 5;
constexpr int CH_H = 7;

std::string norm(std::string_view text);
int measure(std::string_view text, int scale = 1, int spacing = 1);
void draw(Canvas &cv, std::string_view text, int x, int y, int scale, Color color, int spacing = 1);
// Draws with a clip rectangle (used by the marquee / playlist rows).
void drawClipped(Canvas &cv, std::string_view text, Rect clip, int x, int y, int scale,
                 Color color, int spacing = 1);
// Truncates text with ".." so it fits maxW pixels at scale 1 / spacing 1.
std::string ellipsize(std::string_view text, int maxW);

} // namespace pf
