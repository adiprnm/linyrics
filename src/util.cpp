#include "util.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace util {

static int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::string percentDecode(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const int hi = hexVal(s[i + 1]);
            const int lo = hexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(char(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string percentEncodePath(const std::string &s)
{
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c >= 0x80) {
            out.push_back(char(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

bool isFileUrl(const std::string &url)
{
    return url.rfind("file:", 0) == 0;
}

bool isRemoteUrl(const std::string &url)
{
    return !isFileUrl(url) && url.find("://") != std::string::npos;
}

std::string urlToPath(const std::string &url)
{
    if (!isFileUrl(url))
        return url;
    std::string rest = url.substr(5); // after "file:"
    if (rest.rfind("//", 0) == 0) {
        rest = rest.substr(2);
        if (rest.rfind("localhost/", 0) == 0)
            rest = rest.substr(9);
    }
    return percentDecode(rest);
}

std::string fileName(const std::string &pathOrUrl)
{
    std::string p = isFileUrl(pathOrUrl) ? percentDecode(pathOrUrl) : pathOrUrl;
    const size_t slash = p.find_last_of('/');
    if (slash != std::string::npos)
        p = p.substr(slash + 1);
    const size_t query = p.find('?');
    if (query != std::string::npos)
        p = p.substr(0, query);
    return p;
}

std::string lowerExt(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    std::string e = name.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return e;
}

std::string completeBaseName(const std::string &pathOrUrl)
{
    std::string name = fileName(pathOrUrl);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
        name = name.substr(0, dot);
    return name;
}

bool hasAudioExtension(const std::string &name)
{
    static const char *exts[] = {"mp3", "wav", "ogg", "oga", "flac", "m4a", "aac",
                                 "opus", "wma", "aiff", "aif"};
    const std::string e = lowerExt(name);
    for (const char *x : exts)
        if (e == x)
            return true;
    return false;
}

std::vector<std::string> audioFilesInFolder(const std::string &folderPath)
{
    std::vector<std::string> out;
    std::error_code ec;
    fs::recursive_directory_iterator it(folderPath, fs::directory_options::skip_permission_denied, ec);
    if (ec)
        return out;
    for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        const std::string name = it->path().filename().string();
        if (hasAudioExtension(name))
            out.push_back("file://" + percentEncodePath(it->path().string()));
    }
    std::sort(out.begin(), out.end(), [](const std::string &a, const std::string &b) {
        return naturalCompare(fileName(a), fileName(b)) < 0;
    });
    return out;
}

void ensureDir(const std::string &path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
}

std::string configDir()
{
    if (const char *x = std::getenv("XDG_CONFIG_HOME"); x && *x)
        return std::string(x) + "/linyrics";
    if (const char *h = std::getenv("HOME"); h && *h)
        return std::string(h) + "/.config/linyrics";
    return "linyrics";
}

std::string cacheDir()
{
    if (const char *x = std::getenv("XDG_CACHE_HOME"); x && *x)
        return std::string(x) + "/linyrics";
    if (const char *h = std::getenv("HOME"); h && *h)
        return std::string(h) + "/.cache/linyrics";
    return "linyrics-cache";
}

bool fileExists(const std::string &path)
{
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::string readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const std::string &path, const std::string &data)
{
    const fs::path p(path);
    if (p.has_parent_path())
        ensureDir(p.parent_path().string());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(data.data(), std::streamsize(data.size()));
    return bool(out);
}

uint64_t fileSize(const std::string &path)
{
    std::error_code ec;
    const auto s = fs::file_size(path, ec);
    return ec ? 0u : uint64_t(s);
}

int naturalCompare(const std::string &a, const std::string &b)
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const unsigned char ca = (unsigned char)a[i];
        const unsigned char cb = (unsigned char)b[j];
        if (std::isdigit(ca) && std::isdigit(cb)) {
            long long va = 0, vb = 0;
            while (i < a.size() && std::isdigit((unsigned char)a[i]))
                va = va * 10 + (a[i++] - '0');
            while (j < b.size() && std::isdigit((unsigned char)b[j]))
                vb = vb * 10 + (b[j++] - '0');
            if (va != vb)
                return va < vb ? -1 : 1;
            continue;
        }
        const unsigned char la = (unsigned char)std::tolower(ca);
        const unsigned char lb = (unsigned char)std::tolower(cb);
        if (la != lb)
            return la < lb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size())
        return 1;
    if (j < b.size())
        return -1;
    return 0;
}

} // namespace util
