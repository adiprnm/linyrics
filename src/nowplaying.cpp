#include "nowplaying.h"

#include "json.h"
#include "util.h"

#include <cstdio>
#include <cstdlib>

namespace {

int64_t jsonInt(const Json &root, const char *key, int64_t def = 0)
{
    return int64_t(root.num(key, double(def)));
}

} // namespace

std::string NowPlayingReader::filePath()
{
    if (const char *x = std::getenv("XDG_RUNTIME_DIR"); x && *x)
        return std::string(x) + "/linamp/nowplaying.json";
    if (const char *h = std::getenv("HOME"); h && *h)
        return std::string(h) + "/.cache/linamp/nowplaying.json";
    return "nowplaying.json";
}

void NowPlayingReader::poll(int64_t nowMono)
{
    m_changed = false;
    if (m_lastPoll >= 0 && nowMono - m_lastPoll < 70)
        return;
    m_lastPoll = nowMono;

    const std::string data = util::readFile(filePath());
    if (data.empty()) {
        if (m_state.alive) {
            m_state = PlayerState{};
            m_changed = true;
        }
        return;
    }

    bool ok = false;
    const Json root = Json::parse(data, &ok);
    if (!ok || !root.isObject())
        return;

    const int64_t mono = jsonInt(root, "mono");
    const int64_t age = nowMono - mono;
    // The player refreshes at >=1 Hz even when paused; a stale stamp means it
    // was killed (or the session restarted and the runtime dir is stale).
    if (mono <= 0 || age > 2500 || age < -5000) {
        if (m_state.alive) {
            m_state = PlayerState{};
            m_changed = true;
        }
        return;
    }

    const std::string title = root.str("title");
    const std::string artist = root.str("artist");
    const std::string path = root.str("path");
    const bool playing = root.flag("playing");
    const int seq = int(jsonInt(root, "seq"));

    const bool identityChanged = title != m_state.title || artist != m_state.artist ||
                                 path != m_state.path || seq != m_state.seq;
    const bool playChanged = playing != m_state.playing;

    m_state.alive = true;
    m_state.title = title;
    m_state.artist = artist;
    m_state.path = path;
    m_state.duration = jsonInt(root, "duration");
    m_state.seq = seq;
    m_state.playing = playing;
    m_state.basePos = jsonInt(root, "position");
    m_state.baseMono = mono;

    m_changed = identityChanged || playChanged || title.empty();
}

int64_t NowPlayingReader::positionAt(int64_t nowMono) const
{
    const PlayerState &s = m_state;
    if (!s.alive)
        return 0;
    int64_t pos = s.basePos;
    if (s.playing) {
        const int64_t elapsed = nowMono - s.baseMono;
        if (elapsed > 0 && elapsed < 5000)
            pos += elapsed;
    }
    if (s.duration > 0 && pos > s.duration)
        pos = s.duration;
    return pos < 0 ? 0 : pos;
}
