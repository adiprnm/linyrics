#!/usr/bin/env python3
"""Draws assets/icon.png (256x256) without any image library."""
import struct, zlib, math

S = 256
BG = (0x14, 0x18, 0x1F)
EDGE = (0x30, 0x38, 0x44)
DIM = (0xFF, 0xFF, 0xFF, 0.34)
BRIGHT = (0xFF, 0xFF, 0xFF, 0.97)
GREEN = (0x46, 0xFF, 0x5E, 0.95)

px = [[(0, 0, 0, 0)] * S for _ in range(S)]

def blend(dst, src):
    r, g, b, a = src
    if a <= 0: return dst
    dr, dg, db, da = dst
    na = a + da * (1 - a)
    if na <= 0: return (0, 0, 0, 0)
    return (int((r * a + dr * da * (1 - a)) / na),
            int((g * a + dg * da * (1 - a)) / na),
            int((b * a + db * da * (1 - a)) / na), na)

def rrect(x0, y0, x1, y1, rad, color):
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            dx = max(x0 + rad - x, 0, x - (x1 - rad - 1))
            dy = max(y0 + rad - y, 0, y - (y1 - rad - 1))
            d = math.hypot(dx, dy)
            cov = 1.0 if d <= rad - 1 else max(0.0, min(1.0, rad - d))
            if cov <= 0: continue
            r, g, b, a = color
            px[y][x] = blend(px[y][x], (r, g, b, a * cov))

# panel
rrect(8, 8, S - 8, S - 8, 52, (*BG, 1.0))
rrect(9, 9, S - 9, S - 9, 51, (*EDGE, 0.55))
rrect(11, 11, S - 11, S - 11, 49, (*BG, 1.0))

# three lyric lines: dim / active (karaoke sweep) / dim
rrect(64, 74, 192, 88, 7, DIM)
rrect(38, 108, 218, 126, 9, DIM)         # unsung
rrect(38, 108, 152, 126, 9, BRIGHT)      # sung half
rrect(38, 140, 140, 146, 3, GREEN)       # karaoke progress bar
rrect(56, 164, 200, 178, 7, DIM)

def chunk(tag, data):
    c = tag + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

raw = b''
for y in range(S):
    row = bytearray([0])
    for r, g, b, a in px[y]:
        row += bytes((int(max(0, min(255, r))), int(max(0, min(255, g))),
                      int(max(0, min(255, b))), int(max(0.0, min(1.0, a)) * 255)))
    raw += bytes(row)
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', S, S, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw, 9))
       + chunk(b'IEND', b''))
open(__file__.rsplit('/', 1)[0] + '/icon.png', 'wb').write(png)
print('wrote icon.png')
