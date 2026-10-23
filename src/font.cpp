#include "font.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace pf {
namespace {

struct Glyph {
    char ch;
    const char *rows; // 7 rows, 5 chars each, separated by '/'
};

/* Ported verbatim from qml/PixelFont.js */
const Glyph GLYPHS[] = {
    {' ', "00000/00000/00000/00000/00000/00000/00000"},
    {'A', "01110/10001/10001/11111/10001/10001/10001"},
    {'B', "11110/10001/10001/11110/10001/10001/11110"},
    {'C', "01110/10001/10000/10000/10000/10001/01110"},
    {'D', "11110/10001/10001/10001/10001/10001/11110"},
    {'E', "11111/10000/10000/11110/10000/10000/11111"},
    {'F', "11111/10000/10000/11110/10000/10000/10000"},
    {'G', "01110/10001/10000/10111/10001/10001/01111"},
    {'H', "10001/10001/10001/11111/10001/10001/10001"},
    {'I', "11111/00100/00100/00100/00100/00100/11111"},
    {'J', "00111/00010/00010/00010/00010/10010/01100"},
    {'K', "10001/10010/10100/11000/10100/10010/10001"},
    {'L', "10000/10000/10000/10000/10000/10000/11111"},
    {'M', "10001/11011/10101/10101/10001/10001/10001"},
    {'N', "10001/11001/10101/10011/10001/10001/10001"},
    {'O', "01110/10001/10001/10001/10001/10001/01110"},
    {'P', "11110/10001/10001/11110/10000/10000/10000"},
    {'Q', "01110/10001/10001/10001/10101/10010/01101"},
    {'R', "11110/10001/10001/11110/10100/10010/10001"},
    {'S', "01111/10000/10000/01110/00001/00001/11110"},
    {'T', "11111/00100/00100/00100/00100/00100/00100"},
    {'U', "10001/10001/10001/10001/10001/10001/01110"},
    {'V', "10001/10001/10001/10001/10001/01010/00100"},
    {'W', "10001/10001/10001/10101/10101/10101/01010"},
    {'X', "10001/10001/01010/00100/01010/10001/10001"},
    {'Y', "10001/10001/01010/00100/00100/00100/00100"},
    {'Z', "11111/00001/00010/00100/01000/10000/11111"},
    {'0', "01110/10001/10001/10001/10001/10001/01110"},
    {'1', "00100/01100/00100/00100/00100/00100/01110"},
    {'2', "01110/10001/00001/00010/00100/01000/11111"},
    {'3', "11111/00010/00100/00010/00001/10001/01110"},
    {'4', "00010/00110/01010/10010/11111/00010/00010"},
    {'5', "11111/10000/11110/00001/00001/10001/01110"},
    {'6', "00110/01000/10000/11110/10001/10001/01110"},
    {'7', "11111/00001/00010/00100/01000/01000/01000"},
    {'8', "01110/10001/10001/01110/10001/10001/01110"},
    {'9', "01110/10001/10001/01111/00001/00010/01100"},
    {'.', "00000/00000/00000/00000/00000/01100/01100"},
    {',', "00000/00000/00000/00000/00110/00100/01000"},
    {':', "00000/01100/01100/00000/01100/01100/00000"},
    {';', "00000/01100/01100/00000/01100/00100/01000"},
    {'-', "00000/00000/00000/11111/00000/00000/00000"},
    {'_', "00000/00000/00000/00000/00000/00000/11111"},
    {'+', "00000/00100/00100/11111/00100/00100/00000"},
    {'/', "00001/00010/00010/00100/01000/01000/10000"},
    {'\\', "10000/01000/01000/00100/00010/00010/00001"},
    {'(', "00010/00100/01000/01000/01000/00100/00010"},
    {')', "01000/00100/00010/00010/00010/00100/01000"},
    {'[', "01110/01000/01000/01000/01000/01000/01110"},
    {']', "01110/00010/00010/00010/00010/00010/01110"},
    {'!', "00100/00100/00100/00100/00100/00000/00100"},
    {'?', "01110/10001/00001/00010/00100/00000/00100"},
    {'\'', "00100/00100/00000/00000/00000/00000/00000"},
    {'"', "01010/01010/00000/00000/00000/00000/00000"},
    {'&', "01100/10010/10100/01000/10101/10010/01101"},
    {'%', "11001/11010/00010/00100/01000/01011/10011"},
    {'*', "00000/10101/01110/11111/01110/10101/00000"},
    {'<', "00010/00100/01000/10000/01000/00100/00010"},
    {'>', "01000/00100/00010/00001/00010/00100/01000"},
    {'=', "00000/00000/11111/00000/11111/00000/00000"},
    {'#', "01010/01010/11111/01010/11111/01010/01010"},
    {'@', "01110/10001/10111/10101/10111/10000/01110"},
    {'|', "00100/00100/00100/00100/00100/00100/00100"},
    {'^', "00100/01010/10001/00000/00000/00000/00000"},
    {'~', "00000/00000/01000/10101/00010/00000/00000"},
    {'$', "00100/01111/10100/01110/00101/11110/00100"},
};

struct GlyphRows {
    std::array<std::array<bool, CH_W>, CH_H> bit{};
};

const std::unordered_map<char, GlyphRows> &table()
{
    static const std::unordered_map<char, GlyphRows> t = [] {
        std::unordered_map<char, GlyphRows> m;
        for (const Glyph &g : GLYPHS) {
            GlyphRows rows;
            const char *p = g.rows;
            for (int r = 0; r < CH_H; ++r) {
                for (int c = 0; c < CH_W; ++c) {
                    rows.bit[size_t(r)][size_t(c)] = (*p == '1');
                    if (*p)
                        ++p;
                }
                if (*p == '/')
                    ++p;
            }
            m[g.ch] = rows;
        }
        return m;
    }();
    return t;
}

const GlyphRows &rowsFor(char ch)
{
    const auto &t = table();
    auto it = t.find(ch);
    if (it != t.end())
        return it->second;
    return t.at('?');
}

/* UTF-8 decode: returns codepoint and advances i */
uint32_t nextCodepoint(std::string_view s, size_t &i)
{
    const unsigned char c = (unsigned char)s[i];
    auto cont = [&](size_t k) { return i + k < s.size() && ((unsigned char)s[i + k] & 0xC0) == 0x80; };
    if (c < 0x80) {
        ++i;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && cont(1)) {
        uint32_t cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && cont(1) && cont(2)) {
        uint32_t cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) |
                      ((unsigned char)s[i + 2] & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) {
        uint32_t cp = ((c & 0x07u) << 18) | (((unsigned char)s[i + 1] & 0x3Fu) << 12) |
                      (((unsigned char)s[i + 2] & 0x3Fu) << 6) | ((unsigned char)s[i + 3] & 0x3Fu);
        i += 4;
        return cp;
    }
    ++i;
    return '?';
}

/* Latin-1 + Latin Extended-A transliteration (the font has no accents). */
char transliterate(uint32_t cp)
{
    switch (cp) {
    // quotes / dashes / symbols (PixelFont.js REPL)
    case 0x2018: case 0x2019: case 0x02BC: case 0x0060: case 0x00B4: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: case 0x2212: return '-';
    case 0x00D7: return 'x';
    case 0x2192: return '>';
    case 0x2190: return '<';
    case 0x00A0: return ' ';
    case 0x00B0: case 0x00B7: return ' ';
    // A
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
    case 0x0100: case 0x0102: case 0x0104: case 0x00E0: case 0x00E1: case 0x00E2:
    case 0x00E3: case 0x00E4: case 0x00E5: case 0x0101: case 0x0103: case 0x0105: return 'A';
    // C
    case 0x00C7: case 0x0106: case 0x0108: case 0x010A: case 0x010C:
    case 0x00E7: case 0x0107: case 0x0109: case 0x010B: case 0x010D: return 'C';
    // D
    case 0x00D0: case 0x010E: case 0x0110: case 0x00F0: case 0x010F: case 0x0111: return 'D';
    // E
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: case 0x0112: case 0x0116:
    case 0x0118: case 0x011A: case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
    case 0x0113: case 0x0117: case 0x0119: case 0x011B: return 'E';
    // G
    case 0x011E: case 0x011F: case 0x0122: case 0x0123: return 'G';
    // I
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: case 0x012A: case 0x012E:
    case 0x0130: case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: case 0x012B:
    case 0x012F: case 0x0131: return 'I';
    // L
    case 0x0139: case 0x013B: case 0x013D: case 0x0141:
    case 0x013A: case 0x013C: case 0x013E: case 0x0142: return 'L';
    // N
    case 0x00D1: case 0x0143: case 0x0147: case 0x00F1: case 0x0144: case 0x0148: return 'N';
    // O
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
    case 0x014C: case 0x0150: case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
    case 0x00F6: case 0x00F8: case 0x014D: case 0x0151: return 'O';
    // R
    case 0x0154: case 0x0156: case 0x0158: case 0x0155: case 0x0157: case 0x0159: return 'R';
    // S
    case 0x015A: case 0x015C: case 0x015E: case 0x0160:
    case 0x015B: case 0x015D: case 0x015F: case 0x0161: return 'S';
    // T
    case 0x0162: case 0x0164: case 0x0166: case 0x0163: case 0x0165: case 0x0167: return 'T';
    // U
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: case 0x016A: case 0x016E:
    case 0x0170: case 0x0172: case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
    case 0x016B: case 0x016F: case 0x0171: case 0x0173: return 'U';
    // Y
    case 0x00DD: case 0x0176: case 0x0178: case 0x00FD: case 0x00FF: case 0x0177: return 'Y';
    // Z
    case 0x0179: case 0x017B: case 0x017D: case 0x017A: case 0x017C: case 0x017E: return 'Z';
    default:
        if (cp < 0x80)
            return char(std::toupper(int(cp)));
        return '?';
    }
}

} // namespace

std::string norm(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = nextCodepoint(text, i);
        if (cp == 0x2026) { // ellipsis
            out += "...";
            continue;
        }
        char c = transliterate(cp);
        if (c == ' ' && !out.empty() && out.back() == ' ')
            continue;
        out.push_back(c);
    }
    return out;
}

int measure(std::string_view text, int scale, int spacing)
{
    const std::string s = norm(text);
    if (s.empty())
        return 0;
    const int n = int(s.size());
    return n * CH_W * scale + (n - 1) * spacing * scale;
}

static void drawNormalized(Canvas &cv, const std::string &str, int x, int y, int scale,
                           Color color, int spacing)
{
    int cx = x;
    for (char ch : str) {
        if (ch != ' ') {
            const GlyphRows &g = rowsFor(ch);
            for (int r = 0; r < CH_H; ++r)
                for (int c = 0; c < CH_W; ++c)
                    if (g.bit[size_t(r)][size_t(c)])
                        cv.fillRect({cx + c * scale, y + r * scale, scale, scale}, color);
        }
        cx += (CH_W + spacing) * scale;
    }
}

void draw(Canvas &cv, std::string_view text, int x, int y, int scale, Color color, int spacing)
{
    drawNormalized(cv, norm(text), x, y, scale, color, spacing);
}

void drawClipped(Canvas &cv, std::string_view text, Rect clip, int x, int y, int scale,
                 Color color, int spacing)
{
    cv.pushClip(clip);
    drawNormalized(cv, norm(text), x, y, scale, color, spacing);
    cv.popClip();
}

std::string ellipsize(std::string_view text, int maxW)
{
    std::string s = norm(text);
    auto width = [](const std::string &t) { return measure(t, 1, 1); };
    if (width(s) <= maxW)
        return s;
    while (s.size() > 1 && width(s + "..") > maxW)
        s.pop_back();
    return s + "..";
}

} // namespace pf
