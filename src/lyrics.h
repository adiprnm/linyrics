/* ------------------------------------------------------------------
   Linyrics — LRC parsing and timed lookups.

   Supports the usual LRC tags ([ar:], [ti:], [al:], [offset:]) and
   enhanced LRC word timings (<mm:ss.xx>), which is what makes a real
   karaoke sweep possible instead of a whole-line wipe.
   ------------------------------------------------------------------ */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct LyricWord {
    int64_t t = 0;  // ms
    size_t byte = 0; // byte offset into LyricLine::text where the word starts
};

struct LyricLine {
    int64_t t = 0; // ms
    std::string text;
    std::vector<LyricWord> words; // empty for plain line-level sync
};

struct Lyrics {
    bool synced = false;
    std::vector<LyricLine> lines;
    std::string source; // "file", "cache", "lrclib", "manual"
    std::string title;
    std::string artist;
    int64_t lrcOffsetMs = 0;

    bool empty() const { return lines.empty(); }

    /* index of the line that should be highlighted at `pos` (ms), -1 before
       the first line starts */
    int indexAt(int64_t pos) const;

    /* 0..1 progress through line `index` */
    float progressAt(int64_t pos, int index) const;

    /* length in bytes of the sung prefix of line `index` (UTF-8 aligned) */
    size_t sungBytesAt(int64_t pos, int index) const;
};

/* Parses LRC (or plain lyrics — then synced == false). Times are normalised
   with the [offset:] tag; the user delay is applied by the caller. */
bool parseLrc(const std::string &data, Lyrics &out);

/* Tag hygiene for lookups: "Rahasia Hati (Official Music Video)" ->
   "Rahasia Hati", "element band" -> "element". */
std::string cleanTitle(const std::string &title, const std::string &artist);
std::string cleanArtist(const std::string &artist);
