#include "../src/lyrics.h"
#include <cstdio>
#include <cassert>

static void show(const char *name, const char *lrc, int64_t pos) {
    Lyrics l;
    const bool ok = parseLrc(lrc, l);
    const int idx = l.indexAt(pos);
    printf("[%s] ok=%d synced=%d lines=%zu idx=%d prog=%.2f sung=%zu",
           name, ok, l.synced, l.lines.size(), idx, l.progressAt(pos, idx),
           l.sungBytesAt(pos, idx));
    if (idx >= 0) printf(" text='%s' t=%lld", l.lines[idx].text.c_str(), (long long)l.lines[idx].t);
    printf("\n");
}

int main() {
    // basic
    show("basic", "[ar:X]\n[00:01.00]one\n[00:03.50]two\n[00:05.00]three\n", 2000);
    // offset tag: +500 shifts everything earlier
    show("offset", "[offset:500]\n[00:02.00]line\n", 1500);
    // multiple timestamps -> repeated chorus
    show("multi", "[00:01.00][00:10.00]chorus\n[00:05.00]verse\n", 6000);
    // enhanced word timings
    show("words", "[00:10.00]<00:10.00>Hello <00:11.00>world <00:12.50>again\n[00:15.00]next\n", 11500);
    // plain (no timing)
    show("plain", "just\nsome\nwords\n", 1000);
    // before first line
    show("early", "[00:10.00]late\n", 1000);
    // BOM + crlf + blank line
    show("bom", "\xEF\xBB\xBF[ti:T]\r\n[00:01.00]a\r\n\r\n[00:02.00]b\r\n", 1500);
    // fractional + 3-digit ms + h:mm:ss
    show("frac", "[00:01.123]a\n[01:02:03.5]b\n", 1123);
    // last line progress: no next line -> 4s window
    show("last", "[00:10.00]final\n", 12000);

    Lyrics l;
    parseLrc("[00:10.00]<00:10.00>Hello <00:11.00>world <00:12.50>again\n[00:15.00]next\n", l);
    for (int64_t p : {10000, 10500, 11000, 11500, 12000, 12500, 14000, 15000, 16000}) {
        printf("  pos=%lld sung=%zu/%zu\n", (long long)p, l.sungBytesAt(p, 0), l.lines[0].text.size());
    }
    return 0;
}
