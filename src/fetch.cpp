#include "fetch.h"

#include "json.h"
#include "lyrics.h"

#include <curl/curl.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr const char *UA = "Linyrics/1.0 (https://github.com/adiprnm/linyrics)";
constexpr const char *BASE = "https://lrclib.net/api";

size_t writeCb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string urlEncode(const std::string &s)
{
    char *esc = curl_easy_escape(nullptr, s.c_str(), int(s.size()));
    if (!esc)
        return s;
    std::string out(esc);
    curl_free(esc);
    return out;
}

int progressCb(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return (*static_cast<std::atomic<bool> *>(clientp)) ? 1 : 0;
}

bool httpGet(const std::string &url, std::string &body, long &status,
             std::atomic<bool> &cancel)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return false;
    body.clear();
    status = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, UA);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK;
}

/* Fills `res` from an LRCLIB lyric object. */
void readLyricObject(const Json &obj, FetchResult &res)
{
    const Json *synced = obj.find("syncedLyrics");
    const Json *plain = obj.find("plainLyrics");
    res.title = obj.str("trackName");
    res.artist = obj.str("artistName");
    if (synced && synced->type == Json::Type::String && !synced->string_.empty()) {
        res.lrc = synced->string_;
        res.synced = true;
        res.found = true;
    } else if (plain && plain->type == Json::Type::String && !plain->string_.empty()) {
        res.lrc = plain->string_;
        res.synced = false;
        res.found = true;
    }
}

} // namespace

Fetcher::Fetcher()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_thread = std::thread([this] { run(); });
}

Fetcher::~Fetcher()
{
    m_cancel = true; // abort whatever curl is doing right now
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_abort = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    curl_global_cleanup();
}

void Fetcher::request(int id, const std::string &title, const std::string &artist,
                      int64_t durationMs)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_job.id = id;
        m_job.title = title;
        m_job.artist = artist;
        m_job.durationMs = durationMs;
        m_hasJob = true;
    }
    m_cv.notify_all();
}

bool Fetcher::take(FetchResult &out)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hasResult)
        return false;
    out = std::move(m_result);
    m_result = FetchResult{};
    m_hasResult = false;
    return true;
}

void Fetcher::run()
{
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_abort || m_hasJob; });
            if (m_abort)
                return;
            job = m_job;
            m_hasJob = false;
        }

        FetchResult res = fetch(job);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_abort)
                return;
            m_result = std::move(res);
            m_hasResult = true;
        }
    }
}

FetchResult Fetcher::fetch(const Job &job)
{
    FetchResult res;
    res.id = job.id;

    // Tags in the wild carry "(Official Music Video)", "feat. ..." and the
    // artist name inside the title; clean them up before asking the API.
    const std::string title = cleanTitle(job.title, job.artist);
    const std::string artist = cleanArtist(job.artist);

    FetchResult plain; // best plain-text result seen so far
    plain.id = job.id;

    bool tried = false;
    bool reachable = false;

    auto get = [&](const std::string &url) -> bool {
        std::string body;
        long status = 0;
        tried = true;
        if (!httpGet(url, body, status, m_cancel))
            return false;
        reachable = true;
        if (status != 200)
            return false;
        bool ok = false;
        const Json root = Json::parse(body, &ok);
        if (!ok || !root.isObject())
            return false;
        FetchResult cand;
        readLyricObject(root, cand);
        if (!cand.found)
            return false;
        if (cand.synced) {
            res = cand;
            res.id = job.id;
            res.ok = true;
            return true;
        }
        if (!plain.found) {
            plain = cand;
            plain.id = job.id;
        }
        return false;
    };

    auto search = [&](const std::string &url) -> bool {
        std::string body;
        long status = 0;
        tried = true;
        if (!httpGet(url, body, status, m_cancel))
            return false;
        reachable = true;
        if (status != 200)
            return false;
        bool ok = false;
        const Json arr = Json::parse(body, &ok);
        if (!ok || arr.type != Json::Type::Array)
            return false;

        const double want = double(job.durationMs) / 1000.0;
        int bestSynced = -1, bestPlain = -1;
        double bestSyncedScore = 1e18, bestPlainScore = 1e18;
        for (size_t i = 0; i < arr.array.size(); ++i) {
            FetchResult cand;
            readLyricObject(arr.array[i], cand);
            if (!cand.found)
                continue;
            const double dur = arr.array[i].num("duration", 0);
            // no duration (or an unknown one) -> treat as a mediocre match
            const double score = (want > 0 && dur > 0) ? std::fabs(dur - want) : 5.0;
            if (want > 0 && dur > 0 && score > 30.0)
                continue; // a completely different song
            if (cand.synced) {
                if (score < bestSyncedScore) {
                    bestSyncedScore = score;
                    bestSynced = int(i);
                }
            } else if (score < bestPlainScore) {
                bestPlainScore = score;
                bestPlain = int(i);
            }
        }
        if (bestSynced >= 0) {
            readLyricObject(arr.array[size_t(bestSynced)], res);
            res.id = job.id;
            res.ok = true;
            return true;
        }
        if (bestPlain >= 0 && !plain.found) {
            readLyricObject(arr.array[size_t(bestPlain)], plain);
            plain.id = job.id;
        }
        return false;
    };

    auto durationParam = [&](std::string &url) {
        if (job.durationMs <= 0)
            return;
        char buf[32];
        std::snprintf(buf, sizeof buf, "&duration=%lld", (long long)((job.durationMs + 500) / 1000));
        url += buf;
    };

    if (!title.empty()) {
        std::string url = std::string(BASE) + "/get?track_name=" + urlEncode(title);
        if (!artist.empty())
            url += "&artist_name=" + urlEncode(artist);
        durationParam(url);
        if (get(url)) {
            res.ok = true;
            return res;
        }

        if (!artist.empty()) {
            std::string noArtist = std::string(BASE) + "/get?track_name=" + urlEncode(title);
            durationParam(noArtist);
            if (get(noArtist)) {
                res.ok = true;
                return res;
            }
        }

        if (!artist.empty()) {
            const std::string q = std::string(BASE) + "/search?track_name=" + urlEncode(title) +
                                  "&artist_name=" + urlEncode(artist);
            if (search(q)) {
                res.ok = true;
                return res;
            }
        }
        const std::string byTitle = std::string(BASE) + "/search?track_name=" + urlEncode(title);
        if (search(byTitle)) {
            res.ok = true;
            return res;
        }
        if (!artist.empty()) {
            const std::string free = std::string(BASE) + "/search?q=" + urlEncode(artist + " " + title);
            if (search(free)) {
                res.ok = true;
                return res;
            }
        }
    }

    res.ok = true;
    if (plain.found) {
        res = plain;
        res.ok = true;
        return res;
    }
    res.error = (tried && !reachable) ? "network error" : "not found";
    return res;
}
