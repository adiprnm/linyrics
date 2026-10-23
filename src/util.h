/* ------------------------------------------------------------------
   Linamp — small helpers: URL <-> path, file scanning, config dirs
   ------------------------------------------------------------------ */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace util {

std::string percentDecode(const std::string &s);
std::string percentEncodePath(const std::string &s);

/* "file:///home/x/a%20b.mp3" -> "/home/x/a b.mp3"; other schemes pass through */
std::string urlToPath(const std::string &url);
bool isFileUrl(const std::string &url);
bool isRemoteUrl(const std::string &url);

std::string fileName(const std::string &pathOrUrl);
std::string completeBaseName(const std::string &pathOrUrl); // filename without last extension
bool hasAudioExtension(const std::string &name);

/* Recursive scan (sorted, case-insensitive) returning file:// URLs. */
std::vector<std::string> audioFilesInFolder(const std::string &folderPath);

void ensureDir(const std::string &path);
std::string configDir();  // $XDG_CONFIG_HOME/linyrics
std::string cacheDir();   // $XDG_CACHE_HOME/linyrics
bool fileExists(const std::string &path);
std::string readFile(const std::string &path);
bool writeFile(const std::string &path, const std::string &data);
uint64_t fileSize(const std::string &path);

/* lowercase + drop extension */
std::string lowerExt(const std::string &name);

/* Natural sort compare used by the playlist ("track2" < "track10"). */
int naturalCompare(const std::string &a, const std::string &b);

} // namespace util
