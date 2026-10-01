/* ============================================================================
 * MORE -- 分屏显示文件
 *
 * 第一个真正意义的工具：需要 读文件 + 分页 + 等待按键。
 * 顺便说明一个约束：tnd_getline 读到的行会被截断到缓冲区大小，
 * 所以超长行在这里是有损的（v1 接受这个损失）。
 * ==========================================================================*/
#include "tndrt.h"

int tnx_main(void) {
    const char *path;
    char line[512];
    int fd, rows, shown = 0, total = 0;
    tnd_i64 got;

    if (tnd_argc() < 2) {
        tnd_puts("usage: MORE <file>\n");
        return 1;
    }
    path = tnd_argv(1);

    fd = tnd_open(path, TND_O_RDONLY);
    if (fd < 0) { tnd_printf("MORE: cannot open %s\n", path); return 1; }

    rows = tnd_rows() - 2;
    if (rows < 4) rows = 20;

    for (;;) {
        got = tnd_getline(fd, line, sizeof(line));
        if (got < 0) break;          /* < 0 = EOF；0 = 空行，那也是内容 */
        tnd_printf("%s\n", line);
        shown++; total++;
        if (shown >= rows) {
            int k;
            tnd_puts("-- More -- (SPACE continues, Q quits)");
            k = tnd_getkey();
            tnd_puts("\r                                      \r");
            if (TND_KEY(k) == 'q' || TND_KEY(k) == 'Q' || TND_SCAN(k) == TND_S_ESC) break;
            shown = 0;
        }
    }
    tnd_close(fd);
    if (shown) tnd_puts("\n");
    tnd_printf("-- %d line(s) --\n", total);
    return 0;
}
