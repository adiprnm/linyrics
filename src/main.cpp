/* ------------------------------------------------------------------
   Linyrics — entry point.

   One process per session: starting the program again toggles the window
   instead of launching a second copy.

     linyrics            show (or toggle if already running)
     linyrics --toggle   show/hide the running instance
     linyrics --quit     stop the running instance
   ------------------------------------------------------------------ */
#include "app.h"

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t g_toggleRequest = 0;
volatile sig_atomic_t g_quitRequest = 0;

void onSignal(int sig)
{
    if (sig == SIGUSR1)
        g_toggleRequest = 1;
    else
        g_quitRequest = 1;
}

std::string runtimeDir()
{
    if (const char *x = std::getenv("XDG_RUNTIME_DIR"); x && *x)
        return x;
    return "/tmp";
}

std::string pidPath()
{
    return runtimeDir() + "/linyrics.pid";
}

bool readPid(long &pid)
{
    FILE *f = std::fopen(pidPath().c_str(), "r");
    if (!f)
        return false;
    const bool ok = std::fscanf(f, "%ld", &pid) == 1;
    std::fclose(f);
    return ok;
}

/* Returns true when the signal was delivered to a live instance. */
bool signalInstance(int sig)
{
    long pid = 0;
    if (!readPid(pid) || pid <= 0 || pid == long(getpid()))
        return false;
    if (kill(pid_t(pid), 0) != 0)
        return false;
    return kill(pid_t(pid), sig) == 0;
}

void writePidFile()
{
    FILE *f = std::fopen(pidPath().c_str(), "w");
    if (!f)
        return;
    std::fprintf(f, "%ld\n", long(getpid()));
    std::fclose(f);
}

void removePidFile()
{
    long pid = 0;
    if (readPid(pid) && pid == long(getpid()))
        std::remove(pidPath().c_str());
}

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
            std::printf(
                "linyrics %s — floating karaoke lyrics for linamp\n\n"
                "  linyrics            start, or toggle the running window\n"
                "  linyrics --toggle   show/hide the running window\n"
                "  linyrics --quit     stop the running window\n\n"
                "Config: ~/.config/linyrics/config.json\n",
                LINYRICS_VERSION);
            return 0;
        }
    }

    if (wantQuit) {
        if (signalInstance(SIGTERM))
            return 0;
        std::fprintf(stderr, "linyrics: nothing running\n");
        return 1;
    }
    if (wantToggle) {
        if (signalInstance(SIGUSR1))
            return 0;
        std::fprintf(stderr, "linyrics: nothing running\n");
        return 1;
    }

    // already running? just show/hide it instead of starting a second copy
    // (screenshot/self-test runs are exempt: they never open a real window)
    const bool offline = std::getenv("LINYRICS_SCREENSHOT") || std::getenv("LINYRICS_TEST");
    if (!offline && signalInstance(SIGUSR1))
        return 0;

    SDL_SetAppMetadata("Linyrics", LINYRICS_VERSION, "local.linyrics.app");
    SDL_SetHint(SDL_HINT_APP_NAME, "Linyrics");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "linyrics: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    if (!offline)
        writePidFile();

    int exitCode = 0;
    {
        App app;
        if (!app.init()) {
            removePidFile();
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
            while (app.running() && !g_quitRequest) {
                while (SDL_PollEvent(&event))
                    app.handleEvent(event);
                if (!app.running())
                    break;

                if (g_toggleRequest) {
                    g_toggleRequest = 0;
                    app.toggleVisible();
                }

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
        removePidFile();
    SDL_Quit();
    return exitCode;
}
