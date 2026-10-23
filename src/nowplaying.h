/* ------------------------------------------------------------------
   Linyrics — reads what linamp is playing.

   Linamp publishes a tiny JSON snapshot to the runtime dir (see
   linamp/src/nowplaying.cpp). `mono` is CLOCK_MONOTONIC at the instant
   `position` was sampled, which is the same clock in every process on the
   machine — so the exact playback position can be interpolated instead of
   being limited by the publish interval.
   ------------------------------------------------------------------ */
#pragma once

#include <cstdint>
#include <string>

struct PlayerState {
    bool alive = false;
    bool playing = false;
    std::string title;
    std::string artist;
    std::string path; // file:// URL
    int64_t duration = 0;
    int seq = 0;

    // snapshot used for interpolation
    int64_t basePos = 0;
    int64_t baseMono = 0;

    bool hasTrack() const { return alive && !title.empty(); }

    /* identity of the current track (ignores position/play state) */
    std::string key() const { return artist + "\x1f" + title + "\x1f" + path; }
};

class NowPlayingReader {
public:
    /* Reads the snapshot at most every 70 ms. */
    void poll(int64_t nowMono);

    const PlayerState &state() const { return m_state; }
    int64_t positionAt(int64_t nowMono) const;

    /* true when track / play state changed during the last poll() */
    bool changed() const { return m_changed; }

    static std::string filePath();

private:
    PlayerState m_state;
    int64_t m_lastPoll = -1;
    bool m_changed = false;
};
