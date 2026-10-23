/* ------------------------------------------------------------------
   Linyrics — lyric lookup on the network (LRCLIB, no API key).

   Runs on a worker thread so the UI never blocks; the app polls take()
   once per frame. Only the newest request matters: asking for a new
   track replaces whatever was queued.
   ------------------------------------------------------------------ */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct FetchResult {
    int id = -1;
    bool ok = false;    // the request completed (found or not)
    bool found = false; // lyrics payload present
    bool synced = false;
    std::string lrc;    // synced lyrics when synced, else plain text
    std::string title;
    std::string artist;
    std::string error;
};

class Fetcher {
public:
    Fetcher();
    ~Fetcher();

    void request(int id, const std::string &title, const std::string &artist, int64_t durationMs);

    /* Returns true and fills `out` when a result is ready. */
    bool take(FetchResult &out);

private:
    struct Job {
        int id = -1;
        std::string title;
        std::string artist;
        int64_t durationMs = 0;
    };

    void run();
    FetchResult fetch(const Job &job);

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    Job m_job;
    bool m_hasJob = false;
    bool m_abort = false;
    bool m_hasResult = false;
    FetchResult m_result;
    std::atomic<bool> m_cancel{false}; // aborts an in-flight request on exit
};
