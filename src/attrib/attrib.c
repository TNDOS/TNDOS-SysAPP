/* ============================================================================
 * ATTRIB -- 显示（v1 只能显示，不能改）文件属性
 *
 * 说明一件事：为什么 v1 只能读不能写。
 * 改属性要 SetInfo，而 UEFI 的 SetInfo 会连**修改时间一起覆盖** ——
 * 我们得先把原来的时间读出来再原样写回去，否则每改一个只读位就会
 * 顺手改掉文件时间。这个坑不值得现在踩，所以先只做显示。
 * ==========================================================================*/
#include "tndrt.h"

int tnx_main(void) {
    const char *pat = "*.*";
    TND_FIND f;
    int fh, n = 0;

    if (tnd_argc() > 1) pat = tnd_argv(1);

    fh = tnd_findfirst(pat, &f);
    if (fh < 0) { tnd_printf("ATTRIB: %s: nothing matches\n", pat); return 1; }

    do {
        TND_STAT st;
        char flags[6];
        if (tnd_stat(f.Name, &st) != 0) continue;
        flags[0] = (st.Attr & TND_ATTR_DIR)    ? 'D' : '-';
        flags[1] = (st.Attr & TND_ATTR_RDONLY) ? 'R' : '-';
        flags[2] = (f.Attr == 0)               ? '-' : '-';
        flags[3] = 0;
        tnd_printf("  %s  %u   %u-%u-%u %u:%u   %s\n",
                   flags, st.Size, st.Year, st.Month, st.Day, st.Hour, st.Minute, f.Name);
        n++;
    } while (tnd_findnext(fh, &f) == 0);

    tnd_findclose(fh);
    tnd_printf("\n  %d entr(ies).   D=directory  R=read-only\n", n);
    tnd_puts("  (v1 is read-only: writing attributes would also clobber mtime)\n");
    return 0;
}
