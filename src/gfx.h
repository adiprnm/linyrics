/* ------------------------------------------------------------------
   Linamp — software canvas (ARGB8888).
   Everything the UI draws goes through here; one buffer per frame is
   uploaded to a single SDL streaming texture.
   ------------------------------------------------------------------ */
#pragma once

#include "theme.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    Rect inset(int m) const { return {x + m, y + m, w - 2 * m, h - 2 * m}; }
    Rect offset(int dx, int dy) const { return {x + dx, y + dy, w, h}; }
    Rect movedTo(int nx, int ny) const { return {nx, ny, w, h}; }
    Rect intersect(const Rect &o) const
    {
        int x0 = std::max(x, o.x), y0 = std::max(y, o.y);
        int x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
        return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    }
    bool empty() const { return w <= 0 || h <= 0; }
};

struct GradStop {
    float pos;
    Color color;
};

class Canvas {
public:
    /* Logical -> device pixel scale (fractional HiDPI). All coordinates
       passed to the drawing methods are logical; the buffer is in device
       pixels so 1.25x scaling stays sharp instead of being upscaled. */
    void setScale(float sx, float sy);
    float scaleX() const { return sx_; }
    float scaleY() const { return sy_; }
    int toDeviceX(float x) const;
    int toDeviceY(float y) const;
    Rect toDevice(Rect r) const;

    void reset(int w, int h);          // resize buffer and clear to fully transparent
    void clear(Color c);               // overwrite the whole buffer (blend ignored)
    void fillRect(Rect r, Color c);    // alpha-blended fill
    void fillRectRaw(Rect r, Color c); // opaque overwrite
    void vGrad(Rect r, std::initializer_list<GradStop> stops);
    void hLine(int x, int y, int w, Color c);
    void vLine(int x, int y, int h, Color c);
    void outline(Rect r, Color c);
    void plot(int x, int y, Color c);
    void line(int x0, int y0, int x1, int y1, Color c);
    void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c);
    void strokeArc(float cx, float cy, float radius, float a0, float a1, Color c);

    /* Blends an 8-bit coverage mask (antialiased text / icons). */
    void blendMask(int x, int y, int w, int h, const uint8_t *mask, int stride, Color color);

    void pushClip(Rect r);
    void popClip();

    int width() const { return w_; }
    int height() const { return h_; }
    const uint32_t *data() const { return px_.data(); }
    int pitch() const { return w_ * 4; }

private:
    inline void blendPixel(int x, int y, Color c);

    float sx_ = 1.f, sy_ = 1.f;
    int w_ = 0, h_ = 0;
    std::vector<uint32_t> px_;
    Rect clip_;
    std::vector<Rect> clipStack_;
};
