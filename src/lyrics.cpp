#include "lyrics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ')
        ++a;
    while (b > a && (unsigned char)s[b - 1] <= ' ')
        --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}

/* "01:23.45" / "1:23" / "01:02:03.400" -> ms */
bool parseTime(const std::string &raw, int64_t &ms)
{
    const std::string s = trim(raw);
    if (s.empty())
        return false;
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ':') {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    if (parts.size() < 2 || parts.size() > 3)
        return false;

    double total = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string &p = parts[i];
        if (p.empty())
            return false;
        char *end = nullptr;
        const double v = std::strtod(p.c_str(), &end);
        if (!end || *end != '\0' || v < 0)
            return false;
        total = total * 60.0 + v;
    }
    ms = int64_t(std::llround(total * 1000.0));
    return true;
}

void snapUtf8(const std::string &s, size_t &i)
{
    if (i > s.size())
        i = s.size();
    while (i > 0 && i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
        --i;
}

} // namespace

int Lyrics::indexAt(int64_t pos) const
{
    if (lines.empty())
        return -1;
    int lo = 0, hi = int(lines.size()) - 1, best = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (lines[size_t(mid)].t <= pos) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

float Lyrics::progressAt(int64_t pos, int index) const
{
    if (index < 0 || index >= int(lines.size()))
        return 0.f;
    const int64_t t0 = lines[size_t(index)].t;
    int64_t t1 = t0 + 4000;
    if (index + 1 < int(lines.size()) && lines[size_t(index) + 1].t > t0)
        t1 = lines[size_t(index) + 1].t;
    if (pos <= t0)
        return 0.f;
    if (pos >= t1)
        return 1.f;
    return float(double(pos - t0) / double(t1 - t0));
}

size_t Lyrics::sungBytesAt(int64_t pos, int index) const
{
    if (index < 0 || index >= int(lines.size()))
        return 0;
    const LyricLine &L = lines[size_t(index)];
    if (L.text.empty())
        return 0;

    if (L.words.empty()) {
        return size_t(progressAt(pos, index) * double(L.text.size()));
    }

    // enhanced LRC: interpolate between the two surrounding word marks
    const int64_t lineStart = L.t;
    const size_t n = L.words.size();
    size_t k = 0;
    while (k + 1 < n && L.words[k + 1].t <= pos)
        ++k;

    // still before the first word mark: reveal the leading fragment linearly
    if (L.words[0].t > pos) {
        const int64_t span = L.words[0].t - lineStart;
        const double f = span > 0 ? double(pos - lineStart) / double(span) : 0.0;
        size_t b = size_t(std::max(0.0, std::min(1.0, f)) * double(L.words[0].byte));
        snapUtf8(L.text, b);
        return b;
    }

    const int64_t t0 = L.words[k].t;
    const size_t b0 = L.words[k].byte;
    int64_t t1 = (k + 1 < n) ? L.words[k + 1].t : 0;
    if (t1 <= t0) {
        // last word: hold it for a moment, then finish the line
        t1 = (index + 1 < int(lines.size())) ? lines[size_t(index) + 1].t : t0 + 2500;
        if (t1 <= t0)
            t1 = t0 + 2500;
    }
    const size_t b1 = (k + 1 < n) ? L.words[k + 1].byte : L.text.size();
    double f = (pos <= t0) ? 0.0 : double(pos - t0) / double(t1 - t0);
    f = std::max(0.0, std::min(1.0, f));
    size_t b = size_t(double(b0) + f * double(b1 - b0));
    snapUtf8(L.text, b);
    return b;
}

std::string cleanArtist(const std::string &artist)
{
    std::string a = trim(artist);
    // common channel suffixes
    for (const char *suffix : {" - Topic", "VEVO", " Official"}) {
        const std::string sfx = suffix;
        if (a.size() > sfx.size()) {
            const std::string tail = lower(a.substr(a.size() - sfx.size()));
            if (tail == lower(sfx))
                a = trim(a.substr(0, a.size() - sfx.size()));
        }
    }
    const std::string l = lower(a);
    if (l == "various artists" || l == "unknown" || l == "unknown artist")
        return "";
    return a;
}

std::string cleanTitle(const std::string &title, const std::string &artist)
{
    std::string t = trim(title);

    // drop bracketed noise: (Official Music Video), [Lyric Video], {HD}, ...
    std::string out;
    for (size_t i = 0; i < t.size();) {
        const char c = t[i];
        if (c == '(' || c == '[' || c == '{') {
            const char close = c == '(' ? ')' : (c == '[' ? ']' : '}');
            const size_t end = t.find(close, i + 1);
            if (end != std::string::npos) {
                i = end + 1;
                continue;
            }
        }
        out.push_back(c);
        ++i;
    }
    t = trim(out);

    // "feat. X" / "ft. X" up to a separator
    {
        const std::string l = lower(t);
        for (const char *needle : {" feat. ", " feat ", " ft. ", " ft "}) {
            const size_t p = l.find(needle);
            if (p != std::string::npos)
                t = trim(t.substr(0, p));
        }
    }

    // "Artist - Title" -> "Title" when the prefix is the artist
    auto stripArtistPrefix = [&](const std::string &name) {
        if (name.empty())
            return;
        const size_t sep = t.find(" - ");
        if (sep == std::string::npos)
            return;
        const std::string prefix = lower(trim(t.substr(0, sep)));
        const std::string who = lower(trim(name));
        if (prefix.empty() || who.empty())
            return;
        const size_t space = who.find(' ');
        const std::string firstWord = space == std::string::npos ? who : who.substr(0, space);
        if (prefix == who || prefix.find(firstWord) != std::string::npos ||
            who.find(prefix) != std::string::npos)
            t = trim(t.substr(sep + 3));
    };
    stripArtistPrefix(artist);
    if (t.find(" - ") != std::string::npos) {
        // "ELEMENT - Rahasia Hati" with no usable artist tag: the first part
        // is almost always the artist, the last one the song.
        const size_t sep = t.find(" - ");
        const std::string tail = trim(t.substr(sep + 3));
        if (!tail.empty() && tail.size() >= 2)
            t = tail;
    }

    while (!t.empty() && (t.front() == '-' || t.front() == '|'))
        t = trim(t.substr(1));
    while (!t.empty() && (t.back() == '-' || t.back() == '|'))
        t = trim(t.substr(0, t.size() - 1));
    return t;
}

bool parseLrc(const std::string &raw, Lyrics &out)
{
    out = Lyrics{};
    std::string data = raw;
    // BOM
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF)
        data.erase(0, 3);

    std::vector<LyricLine> timed;
    std::vector<std::string> plain;
    int64_t offset = 0;

    size_t pos = 0;
    while (pos <= data.size()) {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos)
            eol = data.size();
        std::string line = data.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<int64_t> stamps;
        size_t i = 0;
        while (i < line.size() && line[i] == '[') {
            const size_t close = line.find(']', i + 1);
            if (close == std::string::npos)
                break;
            const std::string tag = trim(line.substr(i + 1, close - i - 1));
            i = close + 1;

            const size_t colon = tag.find(':');
            if (colon != std::string::npos && colon > 0 &&
                !std::isdigit((unsigned char)tag[0])) {
                const std::string name = lower(tag.substr(0, colon));
                const std::string value = trim(tag.substr(colon + 1));
                if (name == "ar")
                    out.artist = value;
                else if (name == "ti")
                    out.title = value;
                else if (name == "offset") {
                    const long long v = std::atoll(value.c_str());
                    offset = int64_t(v);
                }
                continue;
            }
            int64_t ms = 0;
            if (parseTime(tag, ms))
                stamps.push_back(ms);
        }

        std::string text = line.substr(i);

        // enhanced LRC word timings: <mm:ss.xx>
        std::string clean;
        std::vector<LyricWord> words;
        for (size_t p = 0; p < text.size();) {
            if (text[p] == '<') {
                const size_t close = text.find('>', p + 1);
                int64_t ms = 0;
                if (close != std::string::npos &&
                    parseTime(text.substr(p + 1, close - p - 1), ms)) {
                    words.push_back({ms, clean.size()});
                    p = close + 1;
                    continue;
                }
            }
            clean.push_back(text[p++]);
        }

        if (stamps.empty()) {
            const std::string t = trim(clean);
            if (!t.empty())
                plain.push_back(t);
            continue;
        }
        for (int64_t stamp : stamps) {
            LyricLine ll;
            ll.t = stamp;
            ll.text = trim(clean);
            ll.words = words;
            timed.push_back(std::move(ll));
        }
    }

    out.lrcOffsetMs = offset;

    if (!timed.empty()) {
        std::stable_sort(timed.begin(), timed.end(),
                         [](const LyricLine &a, const LyricLine &b) { return a.t < b.t; });
        for (LyricLine &l : timed) {
            l.t -= offset;
            for (LyricWord &w : l.words)
                w.t -= offset;
        }
        out.synced = true;
        out.lines = std::move(timed);
        return true;
    }

    // no timestamps anywhere: show plain lyrics as-is (no karaoke)
    out.synced = false;
    for (const std::string &t : plain) {
        LyricLine ll;
        ll.text = t;
        out.lines.push_back(std::move(ll));
    }
    return !out.lines.empty();
}
