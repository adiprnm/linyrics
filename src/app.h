/* ------------------------------------------------------------------
   Linyrics — floating karaoke lyrics for linamp.

   A small always-on-top, transparent window that follows whatever linamp
   is playing and draws the lyrics like MiniLyrics does: the current line
   in the middle, the next one below, and a karaoke sweep across the line
   as it is sung.

   Lyrics come from, in order: an .lrc next to the audio file, the local
   cache, then LRCLIB (https://lrclib.net, no API key needed).
   ------------------------------------------------------------------ */
#pragma once

#include "fetch.h"
#include "gfx.h"
#include "lyrics.h"
#include "nowplaying.h"
#include "text.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct MenuRow {
    std::string label;
    std::string value;
    int action = 0;
};

struct Config {
    int width = 880;
    int height = 320;
    int fontPx = 40;
    int delayMs = 0;
    std::string bg = "dim"; // none | dim | dark
    double bgOpacity = 0.72;
    bool online = true;
    bool alwaysOnTop = true;
    bool underline = true;
    bool hideWhenIdle = true;
    std::string fontPath;
};

class App {
public:
    bool init();
    void shutdown();
    void frame();
    bool running() const { return m_running; }
    bool visible() const { return m_visible; }
    void handleEvent(const SDL_Event &e);
    SDL_HitTestResult hitTest(int x, int y) const;
    bool saveScreenshot(const std::string &path);
    void toggleVisible();
    bool wantsFastFrames() const;

    /* called from the (async) file dialog callback */
    void pushDialogFile(const std::string &path);

    /* offline demo content for screenshots / UI work */
    void loadTestContent(const std::string &scene, double positionSec);

    /* headless regression test for the menu (LINYRICS_TEST=1): feeds synthetic
       mouse/keyboard events and checks that the actions actually run. */
    int runSelfTest();

private:
    enum class Status { Idle, Loading, Fetching, Ready, None, Error };

    void loadConfig();
    void saveConfig();
    void updateMetrics();
    void present();
    void draw();
    void drawBackground();
    void drawIdle();
    void drawMessage(const std::string &line1, const std::string &line2);
    void drawPlain();
    void drawLyrics();
    void drawLyricLine(int index, float yCenter, double distance, bool active, int64_t pos);
    void drawToast();
    void drawMenu();

    void resolveLyrics(bool forceOnline);
    void pollFetch();
    bool loadLyricsText(const std::string &text, const std::string &source);
    void requestLyricFile();
    void handleDialogResults();
    std::string cachePath(const std::string &artist, const std::string &title) const;
    std::string statusLabel() const;
    void setToast(const std::string &msg);
    int64_t currentPosition() const;

    void openMenu(float x, float y);
    void closeMenu();
    void runMenuAction(int action);
    int menuItemAt(float x, float y) const;
    int menuRowAt(float x, float y) const;
    Rect menuRect() const;
    int menuContentHeight() const;
    void clampMenuScroll();
    void menuMoveSelection(int dir);
    std::vector<MenuRow> menuEntries(int page) const;

    static SDL_HitTestResult SDLCALL hitTestCb(SDL_Window *, const SDL_Point *, void *);

    SDL_Window *m_win = nullptr;
    SDL_Renderer *m_ren = nullptr;
    SDL_Texture *m_tex = nullptr;
    Canvas m_cv;
    UiText m_font;
    Config m_cfg;

    NowPlayingReader m_player;
    Fetcher m_fetcher;

    Lyrics m_lyrics;
    Status m_status = Status::Idle;
    std::string m_statusText;
    std::string m_trackKey;
    int m_fetchId = 0;

    int m_width = 880, m_height = 220;
    float m_scale = 1.f;

    bool m_debug = false;
    double m_lastDebug = -1;

    double m_now = 0;
    double m_dt = 0;
    double m_lastFrame = 0;
    double m_scroll = 0;
    bool m_scrollInit = false;
    double m_idleSince = -1;

    std::string m_toast;
    double m_toastUntil = 0;

    bool m_menuOpen = false;
    int m_menuPage = 0;
    int m_menuScroll = 0;
    float m_menuX = 0, m_menuY = 0;
    int m_menuHover = -1;
    float m_mouseX = 0, m_mouseY = 0;

    int64_t m_testPos = -1; // forced position for screenshot mode
    bool m_testMode = false;

    bool m_running = true;
    bool m_visible = true;
    bool m_dragging = false;
    bool m_sizeFromUser = false;
    bool m_idleHideSnoozed = false; // user asked to see it while idle

    std::mutex m_dialogMutex;
    std::vector<std::string> m_dialogPaths;
};
