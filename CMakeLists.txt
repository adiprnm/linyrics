# Release build (CI + distributable binaries).
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Two dependency modes:
#   * plain:        SDL3 + libcurl from the distro (fast, for local builds)
#   * CURL_ROOT:    static libcurl from packaging/build-curl.sh (used by CI so
#                   the binary does not drag OpenSSL and friends into the
#                   AppImage). SDL3 is found through CMAKE_PREFIX_PATH and stays
#                   shared, exactly like Linamp does.
cmake_minimum_required(VERSION 3.20)
project(Linyrics VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/app.h
    src/fetch.cpp
    src/fetch.h
    src/lyrics.cpp
    src/lyrics.h
    src/nowplaying.cpp
    src/nowplaying.h
    src/instance.cpp
    src/instance.h
    src/gfx.cpp
    src/gfx.h
    src/font.cpp
    src/font.h
    src/text.cpp
    src/text.h
    src/json.cpp
    src/json.h
    src/util.cpp
    src/util.h
    src/theme.h
    src/third_party/stb_truetype.h
)

add_executable(linyrics ${SOURCES})
target_include_directories(linyrics PRIVATE src src/third_party)
target_compile_definitions(linyrics PRIVATE LINYRICS_VERSION="${PROJECT_VERSION}")
target_compile_options(linyrics PRIVATE -Wall -Wextra -Wno-unused-parameter)

# ---- SDL3 ----------------------------------------------------------------
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
    set(LINYRICS_SDL3 SDL3::SDL3)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    set(LINYRICS_SDL3 PkgConfig::SDL3)
endif()

# ---- libcurl -------------------------------------------------------------
if(CURL_ROOT)
    find_path(CURL_INCLUDE_DIR curl/curl.h HINTS "${CURL_ROOT}/include" REQUIRED)
    find_library(CURL_LIBRARY NAMES curl HINTS "${CURL_ROOT}/lib" REQUIRED)
    target_include_directories(linyrics PRIVATE "${CURL_INCLUDE_DIR}")
    target_compile_definitions(linyrics PRIVATE CURL_STATICLIB)
    if(WIN32)
        # Schannel (built with CURL_TLS=schannel) uses the OS TLS stack, so no
        # OpenSSL and no extra DLLs in the release zip.
        target_link_libraries(linyrics PRIVATE "${CURL_LIBRARY}"
            ws2_32 crypt32 secur32 bcrypt iphlpapi)
    else()
        # Everything stays static so the AppImage only has to carry SDL3
        # (the curl build has compression and HTTP/2 disabled, so no zlib).
        set(OPENSSL_USE_STATIC_LIBS TRUE)
        find_package(OpenSSL REQUIRED)
        find_package(Threads REQUIRED)
        target_link_libraries(linyrics PRIVATE
            "${CURL_LIBRARY}" OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    if(APPLE)
        target_link_libraries(linyrics PRIVATE "-framework CoreFoundation" "-framework Security")
    endif()
else()
    find_package(CURL REQUIRED)
    target_link_libraries(linyrics PRIVATE CURL::libcurl)
endif()

target_link_libraries(linyrics PRIVATE ${LINYRICS_SDL3})

# macOS: keep the deployment target back a bit so older Macs run it.
if(APPLE)
    set_target_properties(linyrics PROPERTIES
        MACOSX_DEPLOYMENT_TARGET "12.0"
        INSTALL_RPATH "@executable_path/../Frameworks")
endif()

# Windows: no console window for a GUI-only app, and no MinGW runtime DLLs.
if(WIN32)
    set_target_properties(linyrics PROPERTIES WIN32_EXECUTABLE ON)
endif()
if(MINGW)
    target_link_options(linyrics PRIVATE -static -static-libgcc -static-libstdc++ -s)
endif()

install(TARGETS linyrics RUNTIME DESTINATION bin)
