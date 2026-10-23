#include "instance.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32

// -------------------------------------------------------------------------
// Windows: named mutex + auto-reset events. No console, no signals.
// -------------------------------------------------------------------------
#include <windows.h>

namespace {

constexpr wchar_t MUTEX_NAME[] = L"Local\\linyrics-single-instance";
constexpr wchar_t TOGGLE_NAME[] = L"Local\\linyrics-toggle";
constexpr wchar_t QUIT_NAME[] = L"Local\\linyrics-quit";

HANDLE g_mutex = nullptr;
HANDLE g_toggle = nullptr;
HANDLE g_quit = nullptr;

std::atomic<bool> g_toggleRequest{false};
std::atomic<bool> g_quitRequest{false};

} // namespace

bool instance_start()
{
    g_mutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!g_mutex)
        return true; // can't tell; don't block startup
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return false;
    }
    g_toggle = CreateEventW(nullptr, FALSE, FALSE, TOGGLE_NAME);
    g_quit = CreateEventW(nullptr, FALSE, FALSE, QUIT_NAME);
    return true;
}

bool instance_signal(bool quit)
{
    HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, quit ? QUIT_NAME : TOGGLE_NAME);
    if (!ev)
        return false;
    SetEvent(ev);
    CloseHandle(ev);
    return true;
}

void instance_poll()
{
    if (g_toggle && WaitForSingleObject(g_toggle, 0) == WAIT_OBJECT_0)
        g_toggleRequest = true;
    if (g_quit && WaitForSingleObject(g_quit, 0) == WAIT_OBJECT_0)
        g_quitRequest = true;
}

bool instance_take_toggle()
{
    return g_toggleRequest.exchange(false);
}

bool instance_take_quit()
{
    return g_quitRequest.exchange(false);
}

void instance_stop()
{
    if (g_toggle)
        CloseHandle(g_toggle);
    if (g_quit)
        CloseHandle(g_quit);
    if (g_mutex)
        CloseHandle(g_mutex);
    g_toggle = g_quit = g_mutex = nullptr;
}

#else

// -------------------------------------------------------------------------
// POSIX: pid file + signals.
// -------------------------------------------------------------------------
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace {

volatile sig_atomic_t g_toggleFlag = 0;
volatile sig_atomic_t g_quitFlag = 0;

void onSignal(int sig)
{
    if (sig == SIGUSR1)
        g_toggleFlag = 1;
    else
        g_quitFlag = 1;
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

} // namespace

bool instance_start()
{
    long pid = 0;
    if (readPid(pid) && pid > 0) {
        if (kill(pid_t(pid), 0) == 0)
            return false; // still alive
    }
    FILE *f = std::fopen(pidPath().c_str(), "w");
    if (f) {
        std::fprintf(f, "%ld\n", long(getpid()));
        std::fclose(f);
    }
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    return true;
}

bool instance_signal(bool quit)
{
    long pid = 0;
    if (!readPid(pid) || pid <= 0 || pid == long(getpid()))
        return false;
    if (kill(pid_t(pid), 0) != 0)
        return false;
    return kill(pid_t(pid), quit ? SIGTERM : SIGUSR1) == 0;
}

void instance_poll() {}

bool instance_take_toggle()
{
    if (g_toggleFlag) {
        g_toggleFlag = 0;
        return true;
    }
    return false;
}

bool instance_take_quit()
{
    if (g_quitFlag) {
        g_quitFlag = 0;
        return true;
    }
    return false;
}

void instance_stop()
{
    long pid = 0;
    if (readPid(pid) && pid == long(getpid()))
        std::remove(pidPath().c_str());
}

#endif
