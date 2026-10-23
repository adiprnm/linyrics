#include "gfx.h"

#include <cmath>
#include <cstring>

void Canvas::setScale(float sx, float sy)
{
    sx_ = (sx > 0.f) ? sx : 1.f;
    sy_ = (sy > 0.f) ? sy : 1.f;
}

int Canvas::toDeviceX(float x) const
{
    return int(std::lround(double(x) * double(sx_)));
}

int Canvas::toDeviceY(float y) const
{
    return int(std::lround(double(y) * double(sy_)));
}

Rect Canvas::toDevice(Rect r) const
{
    const int x0 = toDeviceX(float(r.x));
    const int y0 = toDeviceY(float(r.y));
    const int x1 = toDeviceX(float(r.x + r.w));
    const int y1 = toDeviceY(float(r.y + r.h));
    return {x0, y0, x1 - x0, y1 - y0};
}

void Canvas::reset(int w, int h)
{
    if (w_ != w || h_ != h) {
        w_ = w;
        h_ = h;
        px_.assign(size_t(w) * size_t(h), 0u);
    }
    std::fill(px_.begin(), px_.end(), 0u);
    clip_ = {0, 0, w_, h_};
    clipStack_.clear();
}

void Canvas::clear(Color c)
{
    std::fill(px_.begin(), px_.end(), c);
}

inline void Canvas::blendPixel(int x, int y, Color c)
{
    if (!clip_.contains(x, y))
        return;
    const uint32_t a = (c >> 24) & 0xFFu;
    const size_t idx = size_t(y) * size_t(w_) + size_t(x);
    if (a == 0)
        return;
    if (a == 255) {
        px_[idx] = c | 0xFF000000u;
        return;
    }
    const uint32_t dst = px_[idx];
    const uint32_t da = (dst >> 24) & 0xFFu;
    const uint32_t outA = a + da * (255u - a) / 255u;
    auto mix = [&](uint32_t sc, uint32_t dc) {
        const uint32_t sa = a;
        const uint32_t num = sc * sa * 255u + dc * da * (255u - sa);
        const uint32_t den = outA * 255u;
        return den ? (num / den) & 0xFFu : 0u;
    };
    const uint32_t r = mix((c >> 16) & 0xFFu, (dst >> 16) & 0xFFu);
    const uint32_t g = mix((c >> 8) & 0xFFu, (dst >> 8) & 0xFFu);
    const uint32_t b = mix(c & 0xFFu, dst & 0xFFu);
    px_[idx] = (outA << 24) | (r << 16) | (g << 8) | b;
}

void Canvas::fillRect(Rect r, Color c)
{
    r = toDevice(r).intersect(clip_);
    if (r.empty())
        return;
    for (int y = r.y; y < r.bottom(); ++y)
        for (int x = r.x; x < r.right(); ++x)
            blendPixel(x, y, c);
}

void Canvas::fillRectRaw(Rect r, Color c)
{
    r = toDevice(r).intersect(clip_);
    if (r.empty())
        return;
    for (int y = r.y; y < r.bottom(); ++y) {
        uint32_t *row = px_.data() + size_t(y) * size_t(w_);
        for (int x = r.x; x < r.right(); ++x)
            row[x] = c;
    }
}

void Canvas::vGrad(Rect r, std::initializer_list<GradStop> stops)
{
    r = toDevice(r).intersect(clip_);
    if (r.empty() || stops.size() == 0)
        return;
    std::vector<GradStop> list(stops);
    if (list.size() == 1)
        list.push_back(list[0]);
    const float h = float(std::max(1, r.h - 1));
    for (int y = 0; y < r.h; ++y) {
        const float t = float(y) / h;
        // find segment
        size_t i = 0;
        while (i + 2 < list.size() && t > list[i + 1].pos)
            ++i;
        const GradStop &a = list[i];
        const GradStop &b = list[i + 1];
        const float span = (b.pos - a.pos) > 1e-6f ? (b.pos - a.pos) : 1e-6f;
        float k = (t - a.pos) / span;
        k = std::max(0.f, std::min(1.f, k));
        auto lerp = [&](int shift) {
            const float ca = float((a.color >> shift) & 0xFFu);
            const float cb = float((b.color >> shift) & 0xFFu);
            return uint32_t(ca + (cb - ca) * k) & 0xFFu;
        };
        const uint32_t alpha = lerp(24);
        const Color c = (alpha << 24) | (lerp(16) << 16) | (lerp(8) << 8) | lerp(0);
        const int row = r.y + y;
        for (int x = r.x; x < r.right(); ++x)
            blendPixel(x, row, c);
    }
}

void Canvas::hLine(int x, int y, int w, Color c)
{
    fillRect({x, y, w, 1}, c);
}

void Canvas::vLine(int x, int y, int h, Color c)
{
    fillRect({x, y, 1, h}, c);
}

void Canvas::outline(Rect r, Color c)
{
    if (r.w <= 0 || r.h <= 0)
        return;
    hLine(r.x, r.y, r.w, c);
    hLine(r.x, r.bottom() - 1, r.w, c);
    vLine(r.x, r.y, r.h, c);
    vLine(r.right() - 1, r.y, r.h, c);
}

void Canvas::plot(int x, int y, Color c)
{
    blendPixel(toDeviceX(float(x)), toDeviceY(float(y)), c);
}

void Canvas::line(int lx0, int ly0, int lx1, int ly1, Color c)
{
    int x0 = toDeviceX(float(lx0)), y0 = toDeviceY(float(ly0));
    int x1 = toDeviceX(float(lx1)), y1 = toDeviceY(float(ly1));
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        blendPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Canvas::fillTriangle(float lx1, float ly1, float lx2, float ly2, float lx3, float ly3,
                          Color c)
{
    const float x1 = lx1 * sx_, y1 = ly1 * sy_;
    const float x2 = lx2 * sx_, y2 = ly2 * sy_;
    const float x3 = lx3 * sx_, y3 = ly3 * sy_;
    const int minX = (int)std::floor(std::min({x1, x2, x3}));
    const int maxX = (int)std::ceil(std::max({x1, x2, x3}));
    const int minY = (int)std::floor(std::min({y1, y2, y3}));
    const int maxY = (int)std::ceil(std::max({y1, y2, y3}));
    auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    };
    const float area = edge(x1, y1, x2, y2, x3, y3);
    if (std::fabs(area) < 1e-6f)
        return;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = float(x) + 0.5f, py = float(y) + 0.5f;
            const float w0 = edge(x1, y1, x2, y2, px, py) / area;
            const float w1 = edge(x2, y2, x3, y3, px, py) / area;
            const float w2 = edge(x3, y3, x1, y1, px, py) / area;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                blendPixel(x, y, c);
        }
    }
}

void Canvas::strokeArc(float lcx, float lcy, float lradius, float a0, float a1, Color c)
{
    const float cx = lcx * sx_, cy = lcy * sy_, radius = lradius * sx_;
    const int steps = std::max(8, int(std::fabs(a1 - a0) * radius * 3.f));
    int px = 0, py = 0;
    for (int i = 0; i <= steps; ++i) {
        const float t = a0 + (a1 - a0) * float(i) / float(steps);
        const int x = int(std::round(cx + std::cos(t) * radius));
        const int y = int(std::round(cy + std::sin(t) * radius));
        if (i > 0) {
            // already in device pixels: bypass the scale transform
            int x0 = px, y0 = py, x1 = x, y1 = y;
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            for (;;) {
                blendPixel(x0, y0, c);
                if (x0 == x1 && y0 == y1)
                    break;
                const int e2 = 2 * err;
                if (e2 >= dy) {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx) {
                    err += dx;
                    y0 += sy;
                }
            }
        }
        px = x;
        py = y;
    }
}

void Canvas::blendMask(int x, int y, int w, int h, const uint8_t *mask, int stride, Color color)
{
    const uint32_t cr = (color >> 16) & 0xFFu;
    const uint32_t cg = (color >> 8) & 0xFFu;
    const uint32_t cb = color & 0xFFu;
    const uint32_t ca = (color >> 24) & 0xFFu;
    for (int row = 0; row < h; ++row) {
        const int py = y + row;
        if (py < clip_.y || py >= clip_.bottom())
            continue;
        const uint8_t *src = mask + size_t(row) * size_t(stride);
        uint32_t *dstRow = px_.data() + size_t(py) * size_t(w_);
        for (int col = 0; col < w; ++col) {
            const int px = x + col;
            if (px < clip_.x || px >= clip_.right())
                continue;
            const uint32_t cov = src[col];
            if (cov == 0)
                continue;
            const uint32_t a = cov * ca / 255u;
            if (a == 0)
                continue;
            const uint32_t dst = dstRow[px];
            const uint32_t da = (dst >> 24) & 0xFFu;
            const uint32_t outA = a + da * (255u - a) / 255u;
            auto mix = [&](uint32_t sc, uint32_t dc) {
                const uint32_t num = sc * a * 255u + dc * da * (255u - a);
                const uint32_t den = outA * 255u;
                return den ? (num / den) & 0xFFu : 0u;
            };
            const uint32_t r = mix(cr, (dst >> 16) & 0xFFu);
            const uint32_t g = mix(cg, (dst >> 8) & 0xFFu);
            const uint32_t b = mix(cb, dst & 0xFFu);
            dstRow[px] = (outA << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void Canvas::pushClip(Rect r)
{
    clipStack_.push_back(clip_);
    clip_ = toDevice(r).intersect(clip_);
}

void Canvas::popClip()
{
    if (!clipStack_.empty()) {
        clip_ = clipStack_.back();
        clipStack_.pop_back();
    }
}
