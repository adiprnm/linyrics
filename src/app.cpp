#include "app.h"

#include "json.h"
#include "theme.h"
#include "util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

enum {
    ACT_HEADER = -1,
    ACT_SEP = 0,
    ACT_RELOAD = 1,
    ACT_OPEN_FILE,
    ACT_DELAY_MINUS,
    ACT_DELAY_PLUS,
    ACT_FONT_MINUS,
    ACT_FONT_PLUS,
    ACT_BG,
    ACT_MORE,
    ACT_ONLINE,
    ACT_UNDERLINE,
    ACT_TOPMOST,
    ACT_HIDE_IDLE,
    ACT_DELAY_RESET,
    ACT_BACK,
    ACT_HIDE,
    ACT_QUIT,
};

constexpr int MENU_W = 268;
constexpr int MENU_ROW = 20;
constexpr int MENU_SEP = 6;
constexpr int MENU_PAD = 6;
constexpr int MENU_HEADER = 21;

int64_t nowMono()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Color withAlpha(Color c, int alpha)
{
    const uint32_t a = uint32_t(std::max(0, std::min(255, alpha)));
    return (c & 0x00FFFFFFu) | (a << 24);
}

void fillRoundRect(Canvas &cv, Rect r, int radius, Color c)
{
    radius = std::max(0, std::min(radius, std::min(r.w, r.h) / 2));
    for (int y = 0; y < r.h; ++y) {
        int inset = 0;
        if (y < radius) {
            const double dy = double(radius - y) - 0.5;
            inset = radius - int(std::lround(std::sqrt(std::max(0.0, double(radius * radius) - dy * dy))));
        } else if (y >= r.h - radius) {
            const double dy = double(y - (r.h - radius)) + 0.5;
            inset = radius - int(std::lround(std::sqrt(std::max(0.0, double(radius * radius) - dy * dy))));
        }
        cv.fillRect({r.x + inset, r.y + y, r.w - 2 * inset, 1}, c);
    }
}

std::string sanitize(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    bool lastSpace = false;
    for (unsigned char c : s) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
            c = '_';
        if (c < 0x20)
            continue;
        if (c == ' ') {
            if (lastSpace)
                continue;
            lastSpace = true;
        } else {
            lastSpace = false;
        }
        out.push_back(char(c));
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    while (!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    if (out.size() > 90)
        out.resize(90);
    return out;
}

std::string replaceExt(const std::string &path, const char *ext)
{
    const size_t slash = path.find_last_of('/');
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + ext;
    return path.substr(0, dot) + ext;
}

std::string fmtDelay(int ms)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s%.2f s", ms > 0 ? "+" : "", ms / 1000.0);
    return buf;
}

void SDLCALL fileDialogCb(void *userdata, const char *const *filelist, int)
{
    auto *app = static_cast<App *>(userdata);
    if (!app || !filelist)
        return;
    for (int i = 0; filelist[i]; ++i)
        app->pushDialogFile(filelist[i]);
}

} // namespace

/* ==================================================================
   init / config
   ================================================================== */
SDL_HitTestResult SDLCALL App::hitTestCb(SDL_Window *, const SDL_Point *area, void *data)
{
    return static_cast<App *>(data)->hitTest(area->x, area->y);
}

bool App::init()
{
    m_debug = std::getenv("LINYRICS_DEBUG") != nullptr;
    loadConfig();

    if (!m_cfg.fontPath.empty())
        setenv("LINYRICS_FONT", m_cfg.fontPath.c_str(), 1);
    m_font.load();

    m_width = std::max(320, m_cfg.width);
    m_height = std::max(120, m_cfg.height);

    SDL_SetHint(SDL_HINT_APP_ID, "linyrics");
    Uint32 flags = SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE |
                   SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (m_cfg.alwaysOnTop)
        flags |= SDL_WINDOW_ALWAYS_ON_TOP;

    m_win = SDL_CreateWindow("Linyrics", m_width, m_height, flags);
    if (!m_win) {
        std::fprintf(stderr, "linyrics: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(m_win, 360, 130);
    SDL_SetWindowHitTest(m_win, hitTestCb, this);

    m_ren = SDL_CreateRenderer(m_win, nullptr);
    if (!m_ren) {
        std::fprintf(stderr, "linyrics: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(m_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(m_ren, 1);

    updateMetrics();

    m_lastFrame = double(nowMono()) / 1000.0;
    m_now = m_lastFrame;
    return true;
}

void App::shutdown()
{
    saveConfig();
    if (m_tex)
        SDL_DestroyTexture(m_tex);
    m_tex = nullptr;
    if (m_ren)
        SDL_DestroyRenderer(m_ren);
    m_ren = nullptr;
    if (m_win)
        SDL_DestroyWindow(m_win);
    m_win = nullptr;
}

void App::loadConfig()
{
    const std::string data = util::readFile(util::configDir() + "/config.json");
    if (data.empty())
        return;
    bool ok = false;
    const Json root = Json::parse(data, &ok);
    if (!ok || !root.isObject())
        return;

    m_cfg.width = int(root.num("width", m_cfg.width));
    m_cfg.height = int(root.num("height", m_cfg.height));
    m_cfg.fontPx = int(root.num("fontPx", m_cfg.fontPx));
    m_cfg.delayMs = int(root.num("delayMs", m_cfg.delayMs));
    m_cfg.bgOpacity = root.num("bgOpacity", m_cfg.bgOpacity);
    const std::string bg = root.str("bg", m_cfg.bg);
    if (bg == "none" || bg == "dim" || bg == "dark")
        m_cfg.bg = bg;
    m_cfg.online = root.flag("online", m_cfg.online);
    m_cfg.alwaysOnTop = root.flag("alwaysOnTop", m_cfg.alwaysOnTop);
    m_cfg.underline = root.flag("underline", m_cfg.underline);
    m_cfg.hideWhenIdle = root.flag("hideWhenIdle", m_cfg.hideWhenIdle);
    m_cfg.fontPath = root.str("font", m_cfg.fontPath);

    m_cfg.width = std::clamp(m_cfg.width, 320, 4000);
    m_cfg.height = std::clamp(m_cfg.height, 120, 2000);
    m_cfg.fontPx = std::clamp(m_cfg.fontPx, 14, 120);
    m_cfg.bgOpacity = std::clamp(m_cfg.bgOpacity, 0.0, 1.0);
}

void App::saveConfig()
{
    if (m_testMode)
        return; // screenshot runs must not touch the user's config
    Json root = Json::makeObject();
    // Only a size the user actually dragged is worth remembering; the tiled
    // size the compositor imposes on a fresh window is not.
    root.set("width", Json::makeNumber(m_sizeFromUser ? m_width : m_cfg.width));
    root.set("height", Json::makeNumber(m_sizeFromUser ? m_height : m_cfg.height));
    root.set("fontPx", Json::makeNumber(m_cfg.fontPx));
    root.set("delayMs", Json::makeNumber(m_cfg.delayMs));
    root.set("bg", Json::makeString(m_cfg.bg));
    root.set("bgOpacity", Json::makeNumber(m_cfg.bgOpacity));
    root.set("online", Json::makeBool(m_cfg.online));
    root.set("alwaysOnTop", Json::makeBool(m_cfg.alwaysOnTop));
    root.set("underline", Json::makeBool(m_cfg.underline));
    root.set("hideWhenIdle", Json::makeBool(m_cfg.hideWhenIdle));
    if (!m_cfg.fontPath.empty())
        root.set("font", Json::makeString(m_cfg.fontPath));
    util::writeFile(util::configDir() + "/config.json", root.dump(2));
}

void App::updateMetrics()
{
    int ww = 0, wh = 0;
    SDL_GetWindowSize(m_win, &ww, &wh);
    if (ww > 0 && wh > 0) {
        m_width = ww;
        m_height = wh;
    }

    int dw = m_width, dh = m_height;
    if (m_ren) {
        int ow = 0, oh = 0;
        if (SDL_GetRenderOutputSize(m_ren, &ow, &oh) && ow > 0 && oh > 0) {
            dw = ow;
            dh = oh;
        }
    }
    float sx = float(dw) / float(std::max(1, m_width));
    float sy = float(dh) / float(std::max(1, m_height));

    // manual override, also used to emulate a HiDPI screen in tests
    if (const char *env = std::getenv("LINYRICS_PIXEL_SCALE"); env && *env) {
        const float v = float(std::atof(env));
        if (v > 0.1f) {
            sx = sy = v;
            dw = std::max(1, int(std::lround(m_width * v)));
            dh = std::max(1, int(std::lround(m_height * v)));
        }
    }
    if (sx < 0.5f)
        sx = 1.f;
    if (sy < 0.5f)
        sy = 1.f;
    m_scale = sx;

    if (m_cv.width() != dw || m_cv.height() != dh) {
        m_cv.reset(dw, dh);
        if (m_tex)
            SDL_DestroyTexture(m_tex);
        m_tex = SDL_CreateTexture(m_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, dw, dh);
    }
    m_cv.setScale(sx, sy);
    m_font.setPixelScale(sx);
}

/* ==================================================================
   event handling
   ================================================================== */
bool App::wantsFastFrames() const
{
    if (m_testMode || m_menuOpen || m_now < m_toastUntil)
        return true;
    return m_player.state().playing;
}

void App::toggleVisible()
{
    m_visible = !m_visible;
    if (m_visible) {
        SDL_ShowWindow(m_win);
        SDL_RaiseWindow(m_win);
    } else {
        SDL_HideWindow(m_win);
    }
    // Showing it by hand means the user wants to see it, even while idle.
    m_idleSince = m_now;
    m_idleHideSnoozed = true;
}

void App::setToast(const std::string &msg)
{
    m_toast = msg;
    m_toastUntil = m_now + 1.6;
}

SDL_HitTestResult App::hitTest(int x, int y) const
{
    // While a menu is open every click has to reach the app — a DRAGGABLE
    // region makes the compositor start an interactive move instead, and the
    // menu item never sees the click (this is why right-click worked but the
    // items did not).
    if (m_menuOpen)
        return SDL_HITTEST_NORMAL;
    // bottom-right grip for resizing; everywhere else the window is draggable
    if (x >= m_width - 18 && y >= m_height - 18)
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    return SDL_HITTEST_DRAGGABLE;
}

void App::handleEvent(const SDL_Event &e)
{
    switch (e.type) {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data && *e.drop.data)
            pushDialogFile(e.drop.data);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if (m_dragging)
            m_sizeFromUser = true;
        updateMetrics();
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        updateMetrics();
        break;
    case SDL_EVENT_MOUSE_MOTION:
        m_mouseX = e.motion.x;
        m_mouseY = e.motion.y;
        if (m_menuOpen)
            m_menuHover = menuRowAt(m_mouseX, m_mouseY);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        m_mouseX = e.button.x;
        m_mouseY = e.button.y;
        if (e.button.button == SDL_BUTTON_LEFT) {
            m_dragging = true;
            if (e.button.x >= m_width - 18 && e.button.y >= m_height - 18)
                m_sizeFromUser = true;
        }
        if (m_debug)
            std::fprintf(stderr, "click button=%d at=%d,%d menu=%d\n", e.button.button,
                         int(e.button.x), int(e.button.y), int(m_menuOpen));
        if (e.button.button == SDL_BUTTON_RIGHT) {
            if (m_menuOpen)
                closeMenu();
            else
                openMenu(e.button.x, e.button.y);
        } else if (e.button.button == SDL_BUTTON_LEFT && m_menuOpen) {
            const int idx = menuItemAt(e.button.x, e.button.y);
            if (idx >= 0) {
                const auto rows = menuEntries(m_menuPage);
                const int action = (idx < int(rows.size())) ? rows[size_t(idx)].action : 0;
                runMenuAction(action);
            } else {
                closeMenu();
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT)
            m_dragging = false;
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        const float dy = e.wheel.y;
        if (dy == 0.f)
            break;
        if (m_menuOpen && menuRect().contains(int(e.wheel.mouse_x), int(e.wheel.mouse_y))) {
            m_menuScroll -= int(std::lround(dy)) * 28;
            clampMenuScroll();
        } else if (SDL_GetModState() & SDL_KMOD_CTRL) {
            m_cfg.delayMs += (dy > 0 ? 100 : -100);
            setToast("delay " + fmtDelay(m_cfg.delayMs));
        } else {
            m_cfg.fontPx = std::clamp(m_cfg.fontPx + (dy > 0 ? 2 : -2), 14, 120);
        }
        saveConfig();
        break;
    }
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keymod mod = e.key.mod;
        const int step = (mod & SDL_KMOD_SHIFT) ? 2000 : 250;
        switch (e.key.key) {
        case SDLK_ESCAPE:
            if (m_menuOpen)
                closeMenu();
            else
                toggleVisible();
            break;
        case SDLK_Q:
            if (mod & SDL_KMOD_CTRL)
                m_running = false;
            break;
        case SDLK_R:
            resolveLyrics(true);
            setToast("reloading lyrics");
            break;
        case SDLK_M:
            if (m_menuOpen)
                closeMenu();
            else
                openMenu(float(m_width - MENU_W) * 0.5f, 40.f);
            break;
        case SDLK_DOWN:
        case SDLK_UP:
            if (m_menuOpen) {
                menuMoveSelection(e.key.key == SDLK_DOWN ? 1 : -1);
                break;
            }
            m_cfg.fontPx = std::clamp(m_cfg.fontPx + (e.key.key == SDLK_UP ? 2 : -2), 14, 120);
            saveConfig();
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (m_menuOpen) {
                const auto rows = menuEntries(m_menuPage);
                if (m_menuHover >= 0 && m_menuHover < int(rows.size()))
                    runMenuAction(rows[size_t(m_menuHover)].action);
                break;
            }
            break;
        case SDLK_LEFT:
            m_cfg.delayMs -= step;
            setToast("delay " + fmtDelay(m_cfg.delayMs));
            saveConfig();
            break;
        case SDLK_RIGHT:
            m_cfg.delayMs += step;
            setToast("delay " + fmtDelay(m_cfg.delayMs));
            saveConfig();
            break;
        case SDLK_0:
            if (mod & SDL_KMOD_CTRL) {
                m_cfg.delayMs = 0;
                setToast("delay reset");
                saveConfig();
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

void App::pushDialogFile(const std::string &path)
{
    std::lock_guard<std::mutex> lock(m_dialogMutex);
    m_dialogPaths.push_back(path);
}

void App::handleDialogResults()
{
    std::vector<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(m_dialogMutex);
        paths.swap(m_dialogPaths);
    }
    for (const std::string &p : paths) {
        const std::string data = util::readFile(p);
        if (data.empty())
            continue;
        if (loadLyricsText(data, "file")) {
            m_status = Status::Ready;
            m_statusText.clear();
            const PlayerState &st = m_player.state();
            if (st.hasTrack())
                util::writeFile(cachePath(st.artist, st.title), data);
        } else {
            m_status = Status::None;
            m_statusText = "could not read that file";
            setToast("file .lrc tidak terbaca");
        }
    }
}

/* ==================================================================
   lyric resolution
   ================================================================== */
std::string App::cachePath(const std::string &artist, const std::string &title) const
{
    std::string name = sanitize(artist.empty() ? "unknown" : artist) + " - " +
                       sanitize(title.empty() ? "unknown" : title);
    return util::cacheDir() + "/lyrics/" + name + ".lrc";
}

bool App::loadLyricsText(const std::string &text, const std::string &source)
{
    Lyrics lyr;
    if (!parseLrc(text, lyr) || lyr.empty())
        return false;
    lyr.source = source;
    m_lyrics = std::move(lyr);
    m_scrollInit = false;
    return true;
}

void App::resolveLyrics(bool forceOnline)
{
    m_lyrics = Lyrics{};
    m_statusText.clear();
    m_scrollInit = false;
    ++m_fetchId;

    const PlayerState &st = m_player.state();
    if (!st.hasTrack()) {
        m_status = Status::Idle;
        return;
    }

    // 1. an .lrc sitting next to the audio file wins
    if (!st.path.empty()) {
        const std::string audio = util::urlToPath(st.path);
        if (!audio.empty() && audio[0] == '/') {
            const std::string lrc = replaceExt(audio, ".lrc");
            std::string data = util::readFile(lrc);
            if (data.empty())
                data = util::readFile(replaceExt(audio, ".LRC"));
            if (!data.empty() && loadLyricsText(data, "file")) {
                m_status = Status::Ready;
                return;
            }
        }
    }

    // 2. previously downloaded / hand-picked lyrics
    if (!forceOnline) {
        const std::string data = util::readFile(cachePath(st.artist, st.title));
        if (!data.empty() && loadLyricsText(data, "cache")) {
            m_status = Status::Ready;
            return;
        }
    }

    // 3. LRCLIB
    if (m_cfg.online && !st.title.empty()) {
        m_status = Status::Fetching;
        m_fetcher.request(m_fetchId, st.title, st.artist, st.duration);
        return;
    }

    m_status = Status::None;
}

void App::pollFetch()
{
    FetchResult r;
    while (m_fetcher.take(r)) {
        if (r.id != m_fetchId)
            continue; // result for a track we already left
        if (r.found && loadLyricsText(r.lrc, "lrclib")) {
            m_status = Status::Ready;
            const PlayerState &st = m_player.state();
            if (st.hasTrack())
                util::writeFile(cachePath(st.artist, st.title), r.lrc);
        } else {
            m_status = Status::None;
            m_statusText = r.error;
        }
    }
}

void App::requestLyricFile()
{
    static const SDL_DialogFileFilter filters[] = {
        {"Lyrics", "lrc;LRC;txt"},
        {"All files", "*"},
    };
    setToast("pilih file .lrc…");
    SDL_ShowOpenFileDialog(fileDialogCb, this, m_win, filters, 2, nullptr, false);
}

int64_t App::currentPosition() const
{
    if (m_testPos >= 0)
        return m_testPos + m_cfg.delayMs;
    return m_player.positionAt(nowMono()) + m_cfg.delayMs;
}

std::string App::statusLabel() const
{
    const char *src = m_lyrics.source.c_str();
    switch (m_status) {
    case Status::Idle:
        return "no player";
    case Status::Loading:
    case Status::Fetching:
        return "searching lrclib.net";
    case Status::None:
        return "no lyrics";
    case Status::Error:
        return "error";
    case Status::Ready:
        break;
    }
    if (!m_lyrics.synced)
        return std::string(src) + " · plain text";
    return std::string(src) + (m_lyrics.lines.empty() ? "" : " · synced");
}

/* ==================================================================
   frame
   ================================================================== */
void App::frame()
{
    const int64_t mono = nowMono();
    const double now = double(mono) / 1000.0;
    m_dt = std::min(0.1, now - m_lastFrame);
    if (m_dt < 0)
        m_dt = 0;
    m_lastFrame = now;
    m_now = now;

    if (!m_testMode) {
        m_player.poll(mono);
        const PlayerState &st = m_player.state();
        const std::string key = st.hasTrack() ? st.key() : std::string();
        if (key != m_trackKey) {
            m_trackKey = key;
            if (!st.hasTrack()) {
                m_lyrics = Lyrics{};
                m_status = Status::Idle;
                m_statusText.clear();
            } else {
                resolveLyrics(false);
            }
        }
    }

    pollFetch();
    handleDialogResults();

    if (m_debug && m_now - m_lastDebug > 0.5) {
        m_lastDebug = m_now;
        const int active = m_lyrics.synced ? m_lyrics.indexAt(currentPosition()) : -1;
        std::fprintf(stderr,
                     "pos=%lldms active=%d scroll=%.2f synced=%d lines=%zu status=%d src=%s "
                     "visible=%d\n",
                     (long long)currentPosition(), active, m_scroll, int(m_lyrics.synced),
                     m_lyrics.lines.size(), int(m_status), m_lyrics.source.c_str(),
                     int(m_visible));
    }

    // An idle overlay would sit invisibly on top of the desktop and swallow
    // clicks, so it gets out of the way until a track starts again.
    const bool idle = (m_status == Status::Idle);
    if (m_testMode)
        m_idleSince = -1;
    else if (m_cfg.hideWhenIdle) {
        if (idle) {
            if (m_idleSince < 0)
                m_idleSince = m_now;
            if (m_visible && !m_idleHideSnoozed && m_now - m_idleSince > 5.0) {
                closeMenu();
                SDL_HideWindow(m_win);
                m_visible = false;
                std::fprintf(stderr,
                             "linyrics: nothing playing, window hidden for now "
                             "(run 'linyrics --toggle' to show it)\n");
            }
        } else {
            m_idleSince = -1;
            m_idleHideSnoozed = false;
            if (!m_visible) {
                SDL_ShowWindow(m_win);
                SDL_RaiseWindow(m_win);
                m_visible = true;
            }
        }
    }

    if (!m_visible)
        return;

    draw();
    present();
}

void App::present()
{
    if (!m_tex || !m_ren)
        return;
    SDL_UpdateTexture(m_tex, nullptr, m_cv.data(), m_cv.pitch());
    SDL_SetRenderDrawColor(m_ren, 0, 0, 0, 0);
    SDL_RenderClear(m_ren);
    SDL_RenderTexture(m_ren, m_tex, nullptr, nullptr);
    SDL_RenderPresent(m_ren);
}

/* ==================================================================
   drawing
   ================================================================== */
void App::draw()
{
    m_cv.reset(m_cv.width(), m_cv.height());
    drawBackground();

    const PlayerState &st = m_player.state();
    const std::string trackLine =
        st.artist.empty() ? st.title : st.artist + "  —  " + st.title;

    switch (m_status) {
    case Status::Idle:
        drawIdle();
        break;
    case Status::Loading:
        drawMessage("", "loading lyrics…");
        break;
    case Status::Fetching:
        drawMessage(trackLine, "searching lyrics…");
        break;
    case Status::Error:
        drawMessage(trackLine, m_statusText);
        break;
    case Status::None:
        drawMessage(trackLine, "no lyrics found  ·  right-click → open .lrc");
        break;
    case Status::Ready:
        if (m_lyrics.empty())
            drawIdle();
        else if (!m_lyrics.synced)
            drawPlain();
        else
            drawLyrics();
        break;
    }

    // Lirik bisa tampil (mis. dimuat manual) tanpa data posisi dari Linamp —
    // tanpa peringatan, teksnya hanya diam dan terlihat seperti bug.
    if (!m_testMode && m_status == Status::Ready && !m_player.state().alive) {
        const int px = std::max(11, int(std::lround(m_cfg.fontPx * 0.30)));
        const std::string hint =
            "Linamp belum mengirim posisi — restart Linamp (Ctrl+Q, lalu buka lagi)";
        const int lh = m_font.lineHeight(px);
        const int w = m_font.measure(hint, px);
        Rect pill{(m_width - w) / 2 - 14, m_height - lh - 18, w + 28, lh + 10};
        if (pill.x < 8) {
            pill.x = 8;
            pill.w = m_width - 16;
        }
        fillRoundRect(m_cv, pill, pill.h / 2, withAlpha(0x000000, 190));
        m_font.draw(m_cv, hint, (m_width - w) / 2, pill.y + 5, px, withAlpha(0xE8C46A, 240));
    }

    drawToast();
    if (m_menuOpen)
        drawMenu();
}

void App::drawBackground()
{
    const int W = m_width, H = m_height;
    if (m_cfg.bg == "none")
        return;

    const int a = int(std::lround(m_cfg.bgOpacity * 255.0));
    const Color panelColor = withAlpha(0x0A0D12, a);

    if (m_cfg.bg == "dark") {
        fillRoundRect(m_cv, {5, 5, W - 10, H - 10}, 20, panelColor);
        fillRoundRect(m_cv, {7, 7, W - 14, H - 14}, 18, withAlpha(0x141A22, int(a * 0.55)));
        return;
    }

    // "dim": a soft band that fades out at the top and bottom
    m_cv.vGrad({0, 0, W, H},
               {{0.0f, withAlpha(0x05070A, 0)},
                {0.18f, withAlpha(0x05070A, int(a * 0.82))},
                {0.5f, withAlpha(0x05070A, a)},
                {0.82f, withAlpha(0x05070A, int(a * 0.82))},
                {1.0f, withAlpha(0x05070A, 0)}});
}

void App::drawIdle()
{
    const int W = m_width, H = m_height;
    const int px = std::max(12, int(std::lround(m_cfg.fontPx * 0.34)));
    const int w1 = m_font.measure("LINYRICS", px);
    const int w2 = m_font.measure("waiting for linamp…", px);
    const int lh = m_font.lineHeight(px);
    const int cx = W / 2;
    const int cy = H / 2;

    m_font.draw(m_cv, "LINYRICS", cx - w1 / 2, cy - lh, px, withAlpha(0xFFFFFF, 60));
    m_font.draw(m_cv, "waiting for linamp…", cx - w2 / 2, cy + 2, px, withAlpha(0xFFFFFF, 38));
}

void App::drawMessage(const std::string &line1, const std::string &line2)
{
    const int W = m_width, H = m_height;
    const int px = std::max(13, int(std::lround(m_cfg.fontPx * 0.42)));
    const int lh = m_font.lineHeight(px);
    const int cx = W / 2;
    int y = H / 2 - lh;

    if (!line1.empty()) {
        int w = m_font.measure(line1, px);
        if (w > W - 40) {
            const int shrink = int(std::lround(double(px) * double(W - 40) / double(w)));
            const int w2 = m_font.measure(line1, std::max(10, shrink));
            m_font.draw(m_cv, line1, cx - w2 / 2, y - lh / 2, std::max(10, shrink),
                        withAlpha(0xFFFFFF, 150));
        } else {
            m_font.draw(m_cv, line1, cx - w / 2, y - lh / 2, px, withAlpha(0xFFFFFF, 150));
        }
        y += lh + 6;
    }
    const int w = m_font.measure(line2, px);
    m_font.draw(m_cv, line2, cx - w / 2, y, px, withAlpha(0xFFFFFF, 80));
}

void App::drawPlain()
{
    const int W = m_width, H = m_height;
    const int px = std::max(12, int(std::lround(m_cfg.fontPx * 0.46)));
    const int lh = m_font.lineHeight(px) + 4;

    // no timing information: centre the block and show it unhighlighted
    const int maxLines = std::max(1, (H - 24) / lh);
    const int total = int(m_lyrics.lines.size());
    const int shown = std::min(total, maxLines);
    int first = std::max(0, (total - shown) / 2);

    int y = (H - shown * lh) / 2;
    for (int i = first; i < first + shown; ++i) {
        const std::string &text = m_lyrics.lines[size_t(i)].text;
        if (text.empty()) {
            y += lh;
            continue;
        }
        int p = px;
        int w = m_font.measure(text, p);
        if (w > W - 40) {
            p = std::max(10, int(std::lround(double(p) * double(W - 40) / double(w))));
            w = m_font.measure(text, p);
        }
        m_font.draw(m_cv, text, (W - w) / 2, y + 1, p, withAlpha(0x000000, 90));
        m_font.draw(m_cv, text, (W - w) / 2, y, p, withAlpha(0xFFFFFF, 170));
        y += lh;
    }
}

void App::drawLyrics()
{
    const int H = m_height;
    const int64_t pos = currentPosition();
    const int active = m_lyrics.indexAt(pos);

    const double target = double(std::max(0, active));
    if (!m_scrollInit) {
        m_scroll = target;
        m_scrollInit = true;
    }
    const double k = 1.0 - std::exp(-m_dt * 8.5);
    m_scroll += (target - m_scroll) * k;
    if (std::fabs(target - m_scroll) < 0.002)
        m_scroll = target;

    const float gap = float(m_cfg.fontPx) * 1.55f;
    const float cy = float(H) * 0.5f;

    struct Entry {
        int index;
        double dist;
    };
    std::vector<Entry> order;
    const int count = int(m_lyrics.lines.size());
    for (int i = 0; i < count; ++i) {
        const double d = std::fabs(double(i) - m_scroll);
        if (d < 2.9)
            order.push_back({i, d});
    }
    std::sort(order.begin(), order.end(),
              [](const Entry &a, const Entry &b) { return a.dist > b.dist; });

    for (const Entry &e : order) {
        const float y = cy + float(double(e.index) - m_scroll) * gap;
        drawLyricLine(e.index, y, e.dist, e.index == active, pos);
    }
}

void App::drawLyricLine(int index, float yCenter, double distance, bool active, int64_t pos)
{
    const LyricLine &line = m_lyrics.lines[size_t(index)];
    if (line.text.empty())
        return;

    Canvas &cv = m_cv;
    const int W = m_width;

    double closeness = 1.0 - std::min(1.0, distance);
    closeness = std::max(0.0, closeness);
    // MiniLyrics keeps every line at (nearly) the same size and lets
    // brightness do the highlighting; a little scaling keeps the focus.
    int px = int(std::lround(double(m_cfg.fontPx) * (0.85 + 0.15 * closeness)));
    // Without a background panel the text has to survive any wallpaper, so the
    // outer lines stay a bit brighter and fade over a longer distance.
    const double fade = (m_cfg.bg == "none")
                            ? std::pow(std::max(0.0, 1.0 - distance / 3.2), 0.85)
                            : std::pow(std::max(0.0, 1.0 - distance / 2.9), 1.05);
    if (fade <= 0.01)
        return;

    const int margin = std::max(20, W / 16);
    int tw = m_font.measure(line.text, px);
    if (tw > W - 2 * margin && tw > 0) {
        px = std::max(10, int(std::lround(double(px) * double(W - 2 * margin) / double(tw))));
        tw = m_font.measure(line.text, px);
    }

    const int lh = m_font.lineHeight(px);
    const int x = (W - tw) / 2;
    const int y = int(std::lround(yCenter - float(lh) * 0.5f));

    // MiniLyrics look: white letters with a hard black outline, which is what
    // keeps the lyric readable over a bright wallpaper (no need for a heavy
    // background panel).
    // Full transparency means the lyrics sit straight on the wallpaper, so the
    // black outline has to carry the contrast on bright backgrounds too.
    const double outlineStrength = (m_cfg.bg == "none") ? (active ? 1.0 : 0.88)
                                                        : (active ? 0.80 : 0.45);
    // Keep the outline solid (it only fades with the background panel, where
    // contrast is already handled): white fill on a dark wallpaper, black
    // silhouette on a bright one.
    const double outlineFade = (m_cfg.bg == "none") ? (0.45 + 0.55 * fade) : fade;
    const Color outline =
        withAlpha(0x000000, int(std::lround(255.0 * outlineFade * outlineStrength)));
    m_font.drawOutline(cv, line.text, x, y, px, outline);

    const int baseA = int(std::lround(255.0 * fade));
    if (!active) {
        m_font.draw(cv, line.text, x, y, px, withAlpha(0xFFFFFF, int(baseA * 0.70)));
        return;
    }

    m_font.draw(cv, line.text, x, y, px, withAlpha(0xFFFFFF, int(baseA * 0.55)));

    int sungW = 0;
    const size_t sung = m_lyrics.sungBytesAt(pos, index);
    if (sung > 0) {
        const std::string_view sv(line.text);
        const std::string_view prefix = sv.substr(0, std::min(sung, sv.size()));
        sungW = m_font.measure(prefix, px);
        if (sungW > 0) {
            cv.pushClip({x - 1, y - 2, sungW + 1, lh + 4});
            m_font.draw(cv, line.text, x, y, px, withAlpha(0xFFFFFF, baseA));
            cv.popClip();
        }
    }

    if (m_cfg.underline) {
        const int barH = std::max(2, px / 22);
        const int barY = y + lh + std::max(2, px / 12);
        cv.fillRect({x, barY, tw, barH}, withAlpha(0x46FF5E, int(baseA * 0.16)));
        if (sungW > 0)
            cv.fillRect({x, barY, sungW, barH}, withAlpha(0x46FF5E, int(baseA * 0.62)));
    }
}

void App::drawToast()
{
    if (m_toast.empty() || m_now > m_toastUntil)
        return;
    const int px = std::max(11, int(std::lround(m_cfg.fontPx * 0.3)));
    const int w = m_font.measure(m_toast, px);
    const int lh = m_font.lineHeight(px);
    Rect r{m_width / 2 - w / 2 - 12, m_height - lh - 20, w + 24, lh + 8};
    fillRoundRect(m_cv, r, (lh + 8) / 2, withAlpha(0x000000, 150));
    m_font.draw(m_cv, m_toast, r.x + 12, r.y + 4, px, withAlpha(0xFFFFFF, 235));
}

/* ==================================================================
   menu
   ================================================================== */
std::vector<MenuRow> App::menuEntries(int page) const
{
    std::vector<MenuRow> rows;
    if (page == 0) {
        std::string src = m_lyrics.source.empty() ? "—" : m_lyrics.source;
        rows.push_back({statusLabel(), "", ACT_HEADER});
        rows.push_back({"Reload lyrics", src, ACT_RELOAD});
        rows.push_back({"Open .lrc file…", "", ACT_OPEN_FILE});
        rows.push_back({"", "", ACT_SEP});
        rows.push_back({"Later  (+0.5 s)", fmtDelay(m_cfg.delayMs), ACT_DELAY_PLUS});
        rows.push_back({"Earlier  (−0.5 s)", "", ACT_DELAY_MINUS});
        rows.push_back({"Reset delay", "", ACT_DELAY_RESET});
        rows.push_back({"", "", ACT_SEP});
        rows.push_back({"Bigger text", std::to_string(m_cfg.fontPx) + " px", ACT_FONT_PLUS});
        rows.push_back({"Smaller text", "", ACT_FONT_MINUS});
        rows.push_back({"", "", ACT_SEP});
        rows.push_back({"Background", m_cfg.bg, ACT_BG});
        rows.push_back({"Settings…", "", ACT_MORE});
        rows.push_back({"Quit", "", ACT_QUIT});
    } else {
        rows.push_back({"Settings", "", ACT_HEADER});
        rows.push_back({"Online lookup", m_cfg.online ? "on" : "off", ACT_ONLINE});
        rows.push_back({"Karaoke bar", m_cfg.underline ? "on" : "off", ACT_UNDERLINE});
        rows.push_back({"Always on top", m_cfg.alwaysOnTop ? "on" : "off", ACT_TOPMOST});
        rows.push_back({"Hide when idle", m_cfg.hideWhenIdle ? "on" : "off", ACT_HIDE_IDLE});
        rows.push_back({"Reset delay", "", ACT_DELAY_RESET});
        rows.push_back({"", "", ACT_SEP});
        rows.push_back({"Back", "", ACT_BACK});
    }
    return rows;
}

int App::menuContentHeight() const
{
    int h = MENU_PAD * 2;
    for (const MenuRow &r : menuEntries(m_menuPage))
        h += (r.action == ACT_HEADER) ? MENU_HEADER : (r.action == ACT_SEP ? MENU_SEP : MENU_ROW);
    return h;
}

Rect App::menuRect() const
{
    const int contentH = menuContentHeight();
    const int h = std::min(contentH, std::max(80, m_height - 8));
    int x = int(std::lround(m_menuX));
    int y = int(std::lround(m_menuY));
    x = std::clamp(x, 4, std::max(4, m_width - MENU_W - 4));
    y = std::clamp(y, 4, std::max(4, m_height - h - 4));
    return {x, y, MENU_W, h};
}

void App::clampMenuScroll()
{
    const Rect panel = menuRect();
    const int visible = panel.h - MENU_PAD * 2;
    const int maxScroll = std::max(0, menuContentHeight() - MENU_PAD * 2 - visible);
    m_menuScroll = std::clamp(m_menuScroll, 0, maxScroll);
}

void App::openMenu(float x, float y)
{
    m_menuOpen = true;
    m_menuPage = 0;
    m_menuScroll = 0;
    m_menuX = x;
    m_menuY = y;
    m_menuHover = -1;
    menuMoveSelection(1); // keyboard navigation starts on the first item
}

void App::menuMoveSelection(int dir)
{
    const auto rows = menuEntries(m_menuPage);
    const int n = int(rows.size());
    if (n == 0)
        return;

    int idx = m_menuHover;
    for (int step = 0; step < n; ++step) {
        idx += dir;
        if (idx < 0)
            idx = n - 1;
        if (idx >= n)
            idx = 0;
        const int action = rows[size_t(idx)].action;
        if (action != ACT_HEADER && action != ACT_SEP) {
            m_menuHover = idx;
            break;
        }
    }

    // keep the selection visible when the menu has to scroll
    const Rect panel = menuRect();
    int y = panel.y + MENU_PAD - m_menuScroll;
    for (int i = 0; i < n && i < m_menuHover; ++i) {
        const int action = rows[size_t(i)].action;
        y += (action == ACT_HEADER) ? MENU_HEADER : (action == ACT_SEP ? MENU_SEP : MENU_ROW);
    }
    const int rowH = rows[size_t(m_menuHover)].action == ACT_HEADER ? MENU_HEADER
                     : rows[size_t(m_menuHover)].action == ACT_SEP ? MENU_SEP
                                                                   : MENU_ROW;
    if (y < panel.y + MENU_PAD)
        m_menuScroll = std::max(0, m_menuScroll - (panel.y + MENU_PAD - y));
    else if (y + rowH > panel.bottom() - MENU_PAD)
        m_menuScroll += (y + rowH) - (panel.bottom() - MENU_PAD);
    clampMenuScroll();
}

void App::closeMenu()
{
    m_menuOpen = false;
    m_menuHover = -1;
}

int App::menuRowAt(float x, float y) const
{
    const Rect panel = menuRect();
    if (!panel.contains(int(x), int(y)))
        return -1;
    const auto rows = menuEntries(m_menuPage);
    int yy = panel.y + MENU_PAD - m_menuScroll;
    for (size_t i = 0; i < rows.size(); ++i) {
        const int h = (rows[i].action == ACT_HEADER) ? MENU_HEADER
                                                     : (rows[i].action == ACT_SEP ? MENU_SEP
                                                                                  : MENU_ROW);
        if (rows[i].action != ACT_HEADER && rows[i].action != ACT_SEP) {
            if (y >= float(yy) && y < float(yy + h) && yy >= panel.y + MENU_PAD &&
                yy + h <= panel.bottom() - MENU_PAD)
                return int(i);
        }
        yy += h;
    }
    return -1;
}

int App::menuItemAt(float x, float y) const
{
    return menuRowAt(x, y);
}

void App::runMenuAction(int action)
{
    switch (action) {
    case ACT_SEP:
    case ACT_HEADER:
        return;
    case ACT_RELOAD:
        closeMenu();
        resolveLyrics(true);
        setToast("reloading lyrics");
        break;
    case ACT_OPEN_FILE:
        closeMenu();
        requestLyricFile();
        break;
    case ACT_DELAY_MINUS:
        m_cfg.delayMs -= 500;
        setToast("delay " + fmtDelay(m_cfg.delayMs));
        saveConfig();
        break;
    case ACT_DELAY_PLUS:
        m_cfg.delayMs += 500;
        setToast("delay " + fmtDelay(m_cfg.delayMs));
        saveConfig();
        break;
    case ACT_DELAY_RESET:
        m_cfg.delayMs = 0;
        setToast("delay reset");
        saveConfig();
        break;
    case ACT_FONT_MINUS:
        m_cfg.fontPx = std::clamp(m_cfg.fontPx - 2, 14, 120);
        saveConfig();
        break;
    case ACT_FONT_PLUS:
        m_cfg.fontPx = std::clamp(m_cfg.fontPx + 2, 14, 120);
        saveConfig();
        break;
    case ACT_BG:
        m_cfg.bg = m_cfg.bg == "dim" ? "dark" : (m_cfg.bg == "dark" ? "none" : "dim");
        saveConfig();
        break;
    case ACT_MORE:
        m_menuPage = 1;
        m_menuScroll = 0;
        break;
    case ACT_BACK:
        m_menuPage = 0;
        m_menuScroll = 0;
        break;
    case ACT_ONLINE:
        m_cfg.online = !m_cfg.online;
        saveConfig();
        break;
    case ACT_UNDERLINE:
        m_cfg.underline = !m_cfg.underline;
        saveConfig();
        break;
    case ACT_TOPMOST:
        m_cfg.alwaysOnTop = !m_cfg.alwaysOnTop;
        SDL_SetWindowAlwaysOnTop(m_win, m_cfg.alwaysOnTop);
        saveConfig();
        break;
    case ACT_HIDE_IDLE:
        m_cfg.hideWhenIdle = !m_cfg.hideWhenIdle;
        saveConfig();
        break;
    case ACT_HIDE:
        closeMenu();
        toggleVisible();
        break;
    case ACT_QUIT:
        m_running = false;
        break;
    default:
        break;
    }
}

void App::drawMenu()
{
    const Rect panel = menuRect();
    const auto rows = menuEntries(m_menuPage);

    fillRoundRect(m_cv, {panel.x + 2, panel.y + 3, panel.w, panel.h}, 12,
                  withAlpha(0x000000, 120));
    fillRoundRect(m_cv, panel, 12, withAlpha(0x0F1319, 248));
    fillRoundRect(m_cv, {panel.x + 1, panel.y + 1, panel.w - 2, panel.h - 2}, 11,
                  withAlpha(0x171C24, 250));

    const int hover = m_menuHover;

    m_cv.pushClip({panel.x + 1, panel.y + 1, panel.w - 2, panel.h - 2});
    int y = panel.y + MENU_PAD - m_menuScroll;
    for (size_t i = 0; i < rows.size(); ++i) {
        const MenuRow &row = rows[i];
        if (row.action == ACT_SEP) {
            m_cv.fillRect({panel.x + 10, y + 3, panel.w - 20, 1}, withAlpha(0xFFFFFF, 20));
            y += MENU_SEP;
            continue;
        }
        if (row.action == ACT_HEADER) {
            const int px = 11;
            m_font.draw(m_cv, row.label, panel.x + 14,
                        y + (MENU_HEADER - m_font.lineHeight(px)) / 2, px,
                        withAlpha(0x46FF5E, 185));
            if (!row.value.empty()) {
                const int w = m_font.measure(row.value, px);
                m_font.draw(m_cv, row.value, panel.right() - w - 14,
                            y + (MENU_HEADER - m_font.lineHeight(px)) / 2, px,
                            withAlpha(0xFFFFFF, 90));
            }
            y += MENU_HEADER;
            continue;
        }

        if (int(i) == hover)
            fillRoundRect(m_cv, {panel.x + 5, y + 1, panel.w - 10, MENU_ROW - 2}, 6,
                          withAlpha(0xFFFFFF, 24));
        const int px = 12;
        m_font.draw(m_cv, row.label, panel.x + 16, y + (MENU_ROW - m_font.lineHeight(px)) / 2, px,
                    withAlpha(0xFFFFFF, row.action == ACT_QUIT ? 190 : 228));
        if (!row.value.empty()) {
            const int w = m_font.measure(row.value, px);
            m_font.draw(m_cv, row.value, panel.right() - w - 16,
                        y + (MENU_ROW - m_font.lineHeight(px)) / 2, px, withAlpha(0xFFFFFF, 95));
        }
        y += MENU_ROW;
    }
    m_cv.popClip();
}

/* ==================================================================
   headless self test (synthetic input, no window manager involved)
   ================================================================== */
namespace {
void pushMouse(float x, float y, bool down, int button)
{
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = Uint8(button);
    e.button.down = down;
    e.button.x = x;
    e.button.y = y;
    e.button.clicks = 1;
    SDL_PushEvent(&e);
}
void pushKey(SDL_Keycode k)
{
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    SDL_PushEvent(&e);
}
} // namespace

int App::runSelfTest()
{
    int failures = 0;
    auto check = [&](bool ok, const char *what) {
        std::fprintf(stderr, "%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++failures;
    };
    auto pump = [&] {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            handleEvent(ev);
        frame();
    };

    loadTestContent("playing", 12.0);
    m_cfg.delayMs = 0;
    frame();

    // 1. right click opens the menu
    pushMouse(float(m_width) - 40.f, 20.f, true, SDL_BUTTON_RIGHT);
    pump();
    check(m_menuOpen, "right click opens the menu");

    // 2. left click on a row runs its action (delay is applied, menu stays open)
    const auto rows = menuEntries(0);
    int rowIndex = -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].action == ACT_DELAY_PLUS)
            rowIndex = int(i);
    check(rowIndex >= 0, "menu lists the delay action");

    const Rect panel = menuRect();
    int rowY = panel.y + MENU_PAD - m_menuScroll;
    for (int i = 0; i < rowIndex; ++i) {
        const int a = rows[size_t(i)].action;
        rowY += (a == ACT_HEADER) ? MENU_HEADER : (a == ACT_SEP ? MENU_SEP : MENU_ROW);
    }
    pushMouse(float(panel.x + panel.w / 2), float(rowY + MENU_ROW / 2), true, SDL_BUTTON_LEFT);
    pump();
    check(m_cfg.delayMs == 500, "left click on a menu row runs its action");
    check(m_menuOpen, "delay actions keep the menu open");

    // 3. a click outside the panel closes it
    pushMouse(4.f, float(m_height) - 4.f, true, SDL_BUTTON_LEFT);
    pushMouse(4.f, float(m_height) - 4.f, false, SDL_BUTTON_LEFT);
    pump();
    check(!m_menuOpen, "click outside closes the menu");

    // 4. keyboard: M opens, Down/Down -> "Later", Enter runs it
    pushKey(SDLK_M);
    pump();
    check(m_menuOpen, "M opens the menu");
    pushKey(SDLK_DOWN);
    pushKey(SDLK_DOWN);
    pump();
    pushKey(SDLK_RETURN);
    pump();
    check(m_cfg.delayMs == 1000, "keyboard: Down+Enter runs the selected action");

    // 5. Esc closes the menu
    pushKey(SDLK_ESCAPE);
    pump();
    check(!m_menuOpen, "Esc closes the menu");

    // 6. hit test: the menu must receive clicks, the rest stays draggable
    openMenu(10.f, 10.f);
    frame();
    const Rect mp = menuRect();
    check(hitTest(mp.x + 10, mp.y + 10) == SDL_HITTEST_NORMAL, "menu area accepts clicks");
    closeMenu();
    check(hitTest(2, m_height - 2) == SDL_HITTEST_DRAGGABLE, "background stays draggable");

    m_cfg.delayMs = 0;
    std::fprintf(stderr, "%s (%d failure%s)\n", failures ? "SELF TEST FAILED" : "SELF TEST OK",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

/* ==================================================================
   screenshot / demo helpers
   ================================================================== */
bool App::saveScreenshot(const std::string &path)
{
    const int w = m_cv.width();
    const int h = m_cv.height();
    SDL_Surface *surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!surf)
        return false;
    for (int y = 0; y < h; ++y)
        std::memcpy(static_cast<uint8_t *>(surf->pixels) + size_t(y) * size_t(surf->pitch),
                    m_cv.data() + size_t(y) * size_t(w), size_t(w) * 4);
    const bool ok = SDL_SaveBMP(surf, path.c_str());
    SDL_DestroySurface(surf);
    return ok;
}

void App::loadTestContent(const std::string &scene, double positionSec)
{
    m_testMode = true;
    m_testPos = int64_t(positionSec * 1000.0);

    static const char *DEMO_LRC =
        "[ti:Demo Track]\n"
        "[ar:Linyrics]\n"
        "[00:00.50]This is what a lyric line looks like\n"
        "[00:04.00]Waiting for the next one to arrive\n"
        "[00:07.50]The current line is the bright one\n"
        "[00:11.00]Everything else fades into the background\n"
        "[00:15.00]A karaoke sweep runs across the words\n"
        "[00:19.00]So your eyes can follow the singer\n"
        "[00:23.00]Right-click for options, drag to move\n"
        "[00:27.00]That is all there is to it\n"
        "[00:31.00]Enjoy the music\n";

    static const char *CJK_LRC =
        "[00:00.50]日本語のテスト — ひらがな・カタカナ・漢字\n"
        "[00:04.00]한국어 테스트 — 가나다라마바사\n"
        "[00:08.00]Кириллица — проверка шрифта\n"
        "[00:12.00]Ελληνικά — δοκιμή γραμματοσειράς\n"
        "[00:16.00]混合 mixed script — 1234 “quotes” …\n";

    if (scene == "cjk") {
        loadLyricsText(CJK_LRC, "lrclib");
        m_status = Status::Ready;
        m_scrollInit = false;
        return;
    }
    if (scene == "idle") {
        m_status = Status::Idle;
        return;
    }
    if (scene == "none") {
        m_status = Status::None;
        return;
    }
    loadLyricsText(DEMO_LRC, "lrclib");
    m_status = Status::Ready;
    m_scrollInit = false;
    if (scene == "menu" || scene == "menu2") {
        openMenu(float(m_width) - MENU_W - 20.0f, 16.0f);
        if (scene == "menu2")
            m_menuPage = 1;
    }
}


