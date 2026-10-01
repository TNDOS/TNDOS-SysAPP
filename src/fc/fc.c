/* ============================================================================
 * FC -- 逐行比较两个文件
 *
 *   FC <file1> <file2>
 *   FC /B <file1> <file2>     按字节比较
 * ==========================================================================*/
#include "tndrt.h"

static int cmp_lines(const char *a, const char *b) {
    int n = 0;
    while ((a[n] || b[n]) && n < 500) {
        if (a[n] != b[n]) return n + 1;
        n++;
    }
    if (a[n] != b[n]) return n + 1;
    return 0;
}

int tnx_main(void) {
    const char *p1, *p2;
    int binary = 0, argi = 1;
    int fd1, fd2, diff = 0, line = 0;

    if (tnd_argc() > 1 && tnd_stricmp(tnd_argv(1), "/B") == 0) { binary = 1; argi = 2; }
    if (tnd_argc() < argi + 2) {
        tnd_puts("usage: FC [/B] <file1> <file2>\n");
        return 1;
    }
    p1 = tnd_argv(argi);
    p2 = tnd_argv(argi + 1);

    fd1 = tnd_open(p1, TND_O_RDONLY);
    if (fd1 < 0) { tnd_printf("FC: cannot open %s\n", p1); return 1; }
    fd2 = tnd_open(p2, TND_O_RDONLY);
    if (fd2 < 0) { tnd_close(fd1); tnd_printf("FC: cannot open %s\n", p2); return 1; }

    if (binary) {
        unsigned char b1[256], b2[256];
        tnd_i64 n1, n2;
        tnd_i64 off = 0;
        for (;;) {
            n1 = tnd_read(fd1, b1, sizeof(b1));
            n2 = tnd_read(fd2, b2, sizeof(b2));
            if (n1 <= 0 && n2 <= 0) break;
            if (n1 != n2) { tnd_printf("  size mismatch at offset %u\n", (tnd_u32)off); diff++; break; }
            for (tnd_i64 i = 0; i < n1; i++) {
                if (b1[i] != b2[i]) {
                    tnd_printf("  %08X: %02X vs %02X\n", (tnd_u32)(off + i), b1[i], b2[i]);
                    if (++diff > 20) { tnd_puts("  ... too many differences\n"); break; }
                }
            }
            if (diff > 20) break;
            off += n1;
        }
    } else {
        char l1[512], l2[512];
        for (;;) {
            tnd_i64 a = tnd_getline(fd1, l1, sizeof(l1));
            tnd_i64 b = tnd_getline(fd2, l2, sizeof(l2));
            if (a < 0 && b < 0) break;
            line++;
            if (a < 0) { tnd_printf("  line %d: only in %s\n", line, p2); diff++; continue; }
            if (b < 0) { tnd_printf("  line %d: only in %s\n", line, p1); diff++; continue; }
            {
                int at = cmp_lines(l1, l2);
                if (at) {
                    diff++;
                    tnd_printf("  line %d col %d:\n      %s: %s\n      %s: %s\n", line, at, p1, l1, p2, l2);
                }
            }
            if (diff > 20) { tnd_puts("  ... too many differences\n"); break; }
        }
    }

    tnd_close(fd1);
    tnd_close(fd2);

    if (!diff) { tnd_puts("\n  files are identical\n"); return 0; }
    tnd_printf("\n  %d difference(s)\n", diff);
    return 1;
}
