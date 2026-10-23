/* ------------------------------------------------------------------
   Linyrics — entry point.

   One process per session: starting the program again toggles the window
   instead of launching a second copy.

     linyrics            show (or toggle if already running)
     linyrics --toggle   show/hide the running instance
     linyrics --quit     stop the running instance
   ------------------------------------------------------------------ */
#include "app.h"
#include "instance.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

int runScreenshot(App &app)
{
    const char *path = std::getenv("LINYRICS_SCREENSHOT");
    if (!path)
        return 1;
    const char *sceneEnv = std::getenv("LINYRICS_SCENE");
    const char *posEnv = std::getenv("LINYRICS_POS");
    const std::string scene = sceneEnv ? sceneEnv : "playing";
    const double pos = posEnv ? std::atof(posEnv) : 15.6;

    if (scene == "live") {
        // follow the real player (used to verify the linamp -> LRCLIB path)
        for (int i = 0; i < 450; ++i) {
            app.frame();
            SDL_Delay(16);
        }
    } else {
        app.loadTestContent(scene, pos);
        for (int i = 0; i < 30; ++i) {
            app.frame();
            SDL_Delay(16);
        }
    }
    const bool ok = app.saveScreenshot(path);
    std::fprintf(stderr, "screenshot %s: %s\n", path, ok ? "ok" : SDL_GetError());
    return ok ? 0 : 1;
}

void printHelp()
{
    std::printf(
        "linyrics %s — floating karaoke lyrics for linamp\n\n"
        "  linyrics            start, or toggle the running window\n"
        "  linyrics --toggle   show/hide the running window\n"
        "  linyrics --quit     stop the running window\n\n"
        "Config: ~/.config/linyrics/config.json\n",
        LINYRICS_VERSION);
}

} // namespace

int main(int argc, char *argv[])
{
    bool wantToggle = false;
    bool wantQuit = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--toggle") == 0)
            wantToggle = true;
        else if (std::strcmp(argv[i], "--quit") == 0)
            wantQuit = true;
        else if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("linyrics %s\n", LINYRICS_VERSION);
            return 0;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        }
    }

    if (wantQuit) {
        if (instance_signal(true))
            return 0;
        std::fprintf(stderr, "linyrics: nothing running\n");
        return 1;
    }
    if (wantToggle) {
        if (instance_signal(false))
            return 0;
        std::fprintf(stderr, "linyrics: nothing running\n");
        return 1;
    }

    // Screenshot/self-test runs never open a real window, so they must not take
    // the single-instance lock (that used to leave a stale pid file behind).
    const bool offline = std::getenv("LINYRICS_SCREENSHOT") || std::getenv("LINYRICS_TEST");

    if (!offline && !instance_start()) {
        // Already running: just show/hide it.
        instance_signal(false);
        return 0;
    }

    SDL_SetAppMetadata("Linyrics", LINYRICS_VERSION, "local.linyrics.app");
    SDL_SetHint(SDL_HINT_APP_NAME, "Linyrics");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "linyrics: SDL_Init failed: %s\n", SDL_GetError());
        if (!offline)
            instance_stop();
        return 1;
    }

    int exitCode = 0;
    {
        App app;
        if (!app.init()) {
            if (!offline)
                instance_stop();
            SDL_Quit();
            return 1;
        }

        if (std::getenv("LINYRICS_TEST")) {
            exitCode = app.runSelfTest();
        } else if (std::getenv("LINYRICS_SCREENSHOT")) {
            exitCode = runScreenshot(app);
        } else {
            SDL_Event event;
            double lastDraw = 0;
            while (app.running()) {
                while (SDL_PollEvent(&event))
                    app.handleEvent(event);
                if (!app.running())
                    break;

                instance_poll();
                if (instance_take_quit())
                    break;
                if (instance_take_toggle())
                    app.toggleVisible();

                const double now = double(SDL_GetTicksNS()) / 1e9;
                const double interval = app.wantsFastFrames() ? 1.0 / 60.0 : 1.0 / 15.0;
                if (now - lastDraw >= interval) {
                    app.frame();
                    lastDraw = now;
                } else {
                    SDL_Delay(4);
                }
            }
        }
        app.shutdown();
    }

    if (!offline)
        instance_stop();
    SDL_Quit();
    return exitCode;
}
