/* ------------------------------------------------------------------
   Linyrics — theme: colors, formatting helpers.
   Shared with Linamp (same ARGB8888 canvas conventions).
   ------------------------------------------------------------------ */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>

using Color = uint32_t; // 0xAARRGGBB

constexpr Color rgb(uint32_t v, uint32_t a = 255)
{
    return ((a & 0xFFu) << 24) | (v & 0xFFFFFFu);
}

#ifndef LINYRICS_VERSION
#define LINYRICS_VERSION "1.0.0"
#endif

namespace theme {

constexpr double kPi = std::numbers::pi;

inline const char *version() { return LINYRICS_VERSION; }


constexpr Color bg        = rgb(0x0b1115);
constexpr Color text      = rgb(0xd6d6d6);
constexpr Color textDim   = rgb(0x8d8d8d);
constexpr Color lcdGreen  = rgb(0x38ff55);
constexpr Color lcdDim    = rgb(0x1b8f2d);
constexpr Color lcdSoft   = rgb(0x57a35c);
constexpr Color lcdMuted  = rgb(0x37a24b);
constexpr Color panel     = rgb(0x242424);
constexpr Color frame     = rgb(0x8a8a8a);
constexpr Color green     = rgb(0x46ff5e);
constexpr Color yellow    = rgb(0xeae8b4);

inline double clamp(double v, double a, double b) { return std::max(a, std::min(b, v)); }
inline int clampInt(int v, int a, int b) { return std::max(a, std::min(b, v)); }
inline float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

/* Theme.fmt(): "m:ss" or "h:mm:ss" */
inline std::string fmt(double ms)
{
    if (!std::isfinite(ms) || ms < 0)
        ms = 0;
    long long total = (long long)std::floor(ms / 1000.0);
    long long h = total / 3600;
    long long m = (total % 3600) / 60;
    long long s = total % 60;
    char buf[32];
    if (h > 0)
        std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", h, m, s);
    else
        std::snprintf(buf, sizeof buf, "%lld:%02lld", m, s);
    return buf;
}

/* Theme.fmtLong(): "hh:mm:ss" */
inline std::string fmtLong(double ms)
{
    if (!std::isfinite(ms) || ms < 0)
        ms = 0;
    long long total = (long long)std::floor(ms / 1000.0);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld",
                  total / 3600, (total % 3600) / 60, total % 60);
    return buf;
}

/* Theme.signed(): "+3.5" / "-3.5" / " 0.0" */
inline std::string signedNum(double v)
{
    double r = std::round(v * 10.0) / 10.0;
    const char sign = r > 0 ? '+' : (r < 0 ? '-' : ' ');
    char buf[32];
    std::snprintf(buf, sizeof buf, "%c%.1f", sign, std::fabs(r));
    return buf;
}

} // namespace theme
