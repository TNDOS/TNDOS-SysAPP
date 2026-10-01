/* ============================================================================
 * FIND -- 在文件里找文本
 *
 *   FIND <text> <pattern>
 *   FIND /I <text> <pattern>     忽略大小写
 *   FIND /C <text> <pattern>     只报数量
 * ==========================================================================*/
#include "tndrt.h"

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static int contains(const char *hay, const char *needle, int fold) {
    tnd_size nl = tnd_strlen(needle);
    if (!nl) return 1;
    for (; *hay; hay++) {
        tnd_size i;
        for (i = 0; i < nl; i++) {
            char a = hay[i], b = needle[i];
            if (!a) return 0;
            if (fold) { a = lower(a); b = lower(b); }
            if (a != b) break;
        }
        if (i == nl) return 1;
    }
    return 0;
}

int tnx_main(void) {
    const char *text, *pat;
    int fold = 0, countOnly = 0;
    int argi = 1, hits = 0, files = 0;
    TND_FIND f;
    int fh;

    if (tnd_argc() > 1 && tnd_stricmp(tnd_argv(1), "/I") == 0) { fold = 1; argi = 2; }
    if (tnd_argc() > argi && tnd_stricmp(tnd_argv(argi), "/C") == 0) { countOnly = 1; argi++; }

    if (tnd_argc() < argi + 2) {
        tnd_puts("usage: FIND [/I] [/C] <text> <pattern>\n");
        tnd_puts("       pattern is a DOS wildcard, e.g. *.BAT or *\n");
        return 1;
    }
    text = tnd_argv(argi);
    pat  = tnd_argv(argi + 1);

    fh = tnd_findfirst(pat, &f);
    if (fh < 0) { tnd_printf("FIND: %s: no matching files\n", pat); return 1; }

    do {
        char line[512];
        int fd, lineno = 0;
        tnd_i64 got;

        if (f.Attr & TND_ATTR_DIR) continue;
        files++;

        fd = tnd_open(f.Name, TND_O_RDONLY);
        if (fd < 0) continue;

        while ((got = tnd_getline(fd, line, sizeof(line))) >= 0) {
            lineno++;
            if (contains(line, text, fold)) {
                hits++;
                if (!countOnly) tnd_printf("  %s(%d): %s\n", f.Name, lineno, line);
            }
        }
        tnd_close(fd);
    } while (tnd_findnext(fh, &f) == 0);

    tnd_findclose(fh);

    if (countOnly) tnd_printf("  %d occurrence(s)\n", hits);
    else if (!hits) tnd_printf("  not found in %d file(s)\n", files);
    return hits ? 0 : 1;
}
