/* ============================================================================
 * EDIT -- 全屏文本编辑器
 *
 * 它是 SDK 的**验收测试**，不是附带的小工具。
 * 一个全屏编辑器碰得到 API 的每一寸：读键盘（还要方向键的扫描码）、
 * 定位光标、清屏、读写文件、装得下一块文本缓冲。
 *
 * 能只用 SDK 在用户态写出它，就说明这个 API 完整到可以叫操作系统了。
 *
 * v1 的取舍（写清楚）：
 *   - 行式缓冲：256 行 x 128 列。超出的部分读进来就丢，不假装支持。
 *   - 只能编辑短行，没有自动折行。
 *   - F2 保存，ESC 退出；有未保存改动时会拦一下。
 *   - 没有搜索、没有块操作、没有撤销。那些是下一版的事。
 * ==========================================================================*/
#include "tndrt.h"

#define MAXLINES 256
#define MAXCOLS  128
#define S_F2     0x0C

static char buf[MAXLINES][MAXCOLS];
static int  nlines = 0;
static int  cy = 0, cx = 0, top = 0;
static int  dirty = 0;
static char fname[TND_PATH_MAX];
static int  H = 25, W = 80;
static char msg[64];

static void setmsg(const char *m) { tnd_strncpy(msg, m, sizeof(msg)); }

static int textrows(void) { return H - 3; }   /* 标题 1 行 + 状态 1 行 + 提示 1 行 */

/* ------------------------------------------------------------------ 装载 */
static int load(void) {
    char *raw;
    tnd_i64 n;
    int i;

    nlines = 0;
    raw = (char *)tnd_alloc(65536);
    if (!raw) return 0;

    n = tnd_readfile(fname, raw, 65536);
    if (n < 0) { tnd_free(raw); return 0; }

    {
        int col = 0;
        for (i = 0; i < (int)n; i++) {
            char c = raw[i];
            if (c == '\r') continue;
            if (c == '\n') {
                buf[nlines][col] = 0;
                nlines++;
                col = 0;
                if (nlines >= MAXLINES) break;
                continue;
            }
            if (col < MAXCOLS - 1) buf[nlines][col++] = c;
        }
        if (nlines < MAXLINES) { buf[nlines][col] = 0; nlines++; }
    }
    tnd_free(raw);

    if (nlines == 0) { buf[0][0] = 0; nlines = 1; }
    return 1;
}

static int save(void) {
    int fd, i;
    fd = tnd_open(fname, TND_O_WRONLY | TND_O_CREATE | TND_O_TRUNC);
    if (fd < 0) { setmsg("SAVE FAILED"); return 0; }
    for (i = 0; i < nlines; i++) {
        int len = (int)tnd_strlen(buf[i]);
        if (len) tnd_write(fd, buf[i], len);
        tnd_write(fd, "\r\n", 2);
    }
    tnd_close(fd);
    dirty = 0;
    return 1;
}

/* ------------------------------------------------------------------ 编辑 */
static int linelen(int l) { return (int)tnd_strlen(buf[l]); }

static void insertch(int ch) {
    int len = linelen(cy);
    int i;
    if (len >= MAXCOLS - 1) return;
    for (i = len; i >= cx; i--) buf[cy][i + 1] = buf[cy][i];
    buf[cy][cx] = (char)ch;
    cx++;
    dirty = 1;
}

static void backspace(void) {
    int len = linelen(cy);
    int i;
    if (cx > 0) {
        for (i = cx - 1; i < len; i++) buf[cy][i] = buf[cy][i + 1];
        cx--;
        dirty = 1;
    } else if (cy > 0) {
        int prev = linelen(cy - 1);
        int j;
        if (prev + len < MAXCOLS - 1) {
            for (j = 0; j <= len; j++) buf[cy - 1][prev + j] = buf[cy][j];
            for (j = cy; j < nlines - 1; j++) tnd_strncpy(buf[j], buf[j + 1], MAXCOLS);
            nlines--;
            cy--;
            cx = prev;
            dirty = 1;
        }
    }
}

static void delchar(void) {
    int len = linelen(cy);
    int i;
    if (cx < len) {
        for (i = cx; i < len; i++) buf[cy][i] = buf[cy][i + 1];
        dirty = 1;
    } else if (cy + 1 < nlines) {
        int next = linelen(cy + 1);
        int j;
        if (len + next < MAXCOLS - 1) {
            for (j = 0; j <= next; j++) buf[cy][len + j] = buf[cy + 1][j];
            for (j = cy + 1; j < nlines - 1; j++) tnd_strncpy(buf[j], buf[j + 1], MAXCOLS);
            nlines--;
            dirty = 1;
        }
    }
}

static void splitline(void) {
    int len = linelen(cy);
    int i;
    if (nlines >= MAXLINES) return;
    for (i = nlines; i > cy + 1; i--) tnd_strncpy(buf[i], buf[i - 1], MAXCOLS);
    nlines++;
    tnd_strncpy(buf[cy + 1], buf[cy] + cx, MAXCOLS);
    buf[cy][cx] = 0;
    cy++;
    cx = 0;
    dirty = 1;
}

static void clamp(void) {
    int tr = textrows();
    if (cy < 0) cy = 0;
    if (cy >= nlines) cy = nlines - 1;
    if (cy < 0) cy = 0;
    if (cx < 0) cx = 0;
    if (cx > linelen(cy)) cx = linelen(cy);
    if (cy < top) top = cy;
    if (cy >= top + tr) top = cy - tr + 1;
    if (top < 0) top = 0;
}

/* ------------------------------------------------------------------ 绘制 */
/* 三块区域用颜色分开 —— 全屏程序没有颜色就是一片字，分不清哪里是边框。
 * 配色照抄 DOS 的经典组合，常量在 tnd_api.h 里（UEFI 和 VGA 属性字节一致）。 */
#define C_TEXT   TND_ATTR(TND_LIGHTGRAY, TND_BLACK)     /* 正文 */
#define C_TITLE  TND_ATTR(TND_WHITE,     TND_BLUE)      /* 标题栏 */
#define C_HELP   TND_ATTR(TND_BLACK,     TND_CYAN)      /* 操作提示 */
#define C_STATUS TND_ATTR(TND_BLACK,     TND_LIGHTGRAY) /* 状态行 */
#define C_CUR    TND_ATTR(TND_WHITE,     TND_BLUE)      /* 当前行 */

static void draw(void) {
    int i, tr = textrows();
    char row[MAXCOLS + 2];

    tnd_setattr(C_TEXT);
    tnd_cls();

    /* 标题 */
    /* --- 标题栏：白底蓝 --- */
    tnd_setattr(C_TITLE);
    tnd_gotoxy(0, 0);
    {
        char t[160];
        tnd_strncpy(t, " TNDDOS EDIT", sizeof(t));
        if (fname[0]) { tnd_strcat(t, "  --  ", sizeof(t)); tnd_strcat(t, fname, sizeof(t)); }
        if (dirty) tnd_strcat(t, "   [modified]", sizeof(t));
        for (i = (int)tnd_strlen(t); i < W; i++) tnd_strcat(t, " ", sizeof(t));
        tnd_puts(t);
    }

    /* --- 文本区：浅灰 on 黑；当前行反白 --- */
    for (i = 0; i < tr; i++) {
        int li = top + i, j;
        int iscur = (li == cy);

        tnd_setattr(iscur ? C_CUR : C_TEXT);
        tnd_gotoxy(0, 1 + i);

        if (li < nlines) {
            int len = linelen(li);
            for (j = 0; j < W && j < len; j++) row[j] = buf[li][j];
            for (; j < W; j++) row[j] = ' ';
        } else {
            row[0] = '~';                       /* 文件结尾之后的空行标记 */
            for (j = 1; j < W; j++) row[j] = ' ';
        }
        row[W] = 0;
        tnd_puts(row);
    }

    /* --- 提示行：黑底青 --- */
    tnd_setattr(C_HELP);
    tnd_gotoxy(0, 1 + tr);
    {
        char t[160];
        tnd_strncpy(t, " F2 Save    ESC Quit    arrows move    Enter split    Bksp/Del delete", sizeof(t));
        if (msg[0]) { tnd_strcat(t, "    -- ", sizeof(t)); tnd_strcat(t, msg, sizeof(t)); }
        for (i = (int)tnd_strlen(t); i < W; i++) tnd_strcat(t, " ", sizeof(t));
        tnd_puts(t);
    }

    /* --- 状态行：黑底浅灰 --- */
    tnd_setattr(C_STATUS);
    tnd_gotoxy(0, 2 + tr);
    {
        char t[96];
        tnd_strncpy(t, " ", sizeof(t));
        for (i = (int)tnd_strlen(t); i < W; i++) tnd_strcat(t, " ", sizeof(t));
        /* 先铺满再用 gotoxy 写内容，免得残留上一帧的字符 */
        tnd_puts(t);
        tnd_gotoxy(1, 2 + tr);
        tnd_printf("Ln %d/%d    Col %d    %d line(s)", cy + 1, nlines, cx + 1, nlines);
    }

    /* 光标恢复成正文色 */
    tnd_setattr(C_TEXT);
    tnd_gotoxy(cx, 1 + cy - top);
}

/* ------------------------------------------------------------------ 主循环 */
int tnx_main(void) {
    int k, quit = 0;

    if (tnd_argc() > 1) tnd_strncpy(fname, tnd_argv(1), sizeof(fname));

    H = tnd_rows();
    W = tnd_cols();
    if (W > MAXCOLS) W = MAXCOLS;
    if (H < 6) H = 6;

    if (!fname[0]) {
        tnd_puts("usage: EDIT <file>\n");
        return 1;
    }

    if (!load()) setmsg("new file");
    else setmsg("");

    while (!quit) {
        clamp();
        draw();

        k = tnd_getkey();
        msg[0] = 0;

        if (TND_SCAN(k)) {
            int sc = TND_SCAN(k);
            if (sc == TND_S_UP)         cy--;
            else if (sc == TND_S_DOWN)  cy++;
            else if (sc == TND_S_LEFT)  { if (cx > 0) cx--; else if (cy > 0) { cy--; cx = linelen(cy); } }
            else if (sc == TND_S_RIGHT) { if (cx < linelen(cy)) cx++; else if (cy + 1 < nlines) { cy++; cx = 0; } }
            else if (sc == TND_S_HOME)  cx = 0;
            else if (sc == TND_S_END)   cx = linelen(cy);
            else if (sc == TND_S_PGUP)  cy -= textrows();
            else if (sc == TND_S_PGDN)  cy += textrows();
            else if (sc == TND_S_DELETE) delchar();
            else if (sc == S_F2)        { if (save()) setmsg("saved"); else setmsg("SAVE FAILED"); }
            else if (sc == TND_S_ESC)   quit = 1;
        } else {
            int ch = TND_KEY(k);
            if (ch == 0x1B)      quit = 1;
            else if (ch == 13)   splitline();
            else if (ch == 8)    backspace();
            else if (ch == 0x7F) delchar();
            else if (ch >= 32 && ch < 127) insertch(ch);
        }

        if (quit && dirty) {
            tnd_gotoxy(0, 2 + textrows());
            tnd_puts(" Unsaved changes. Press F2 to save, ESC again to discard."          );
            k = tnd_getkey();
            if (TND_SCAN(k) == S_F2) save();
            quit = 1;
            dirty = 0;
        }
    }

    /* 退出时把颜色恢复成 DOS 默认的浅灰 on 黑，
     * 否则 Shell 的提示符会带着 EDIT 的配色继续跑 */
    tnd_setattr(TND_ATTR(TND_LIGHTGRAY, TND_BLACK));
    tnd_cls();
    tnd_printf("\n  EDIT: closed %s\n", fname);
    return 0;
}
