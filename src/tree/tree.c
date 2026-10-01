/* ============================================================================
 * TREE -- 以树形显示目录结构
 *
 * 递归的关键：api_findfirst 会把模式拆成"目录部分 + 名字部分"，
 * 所以只要传 "SUB\\*" 就能遍历子目录，不必先 chdir。
 *
 * 但**必须先收集完这一层再递归** —— 边遍历边开新的目录句柄，
 * 底层 UEFI 的目录读取位置会互相干扰。这是踩过的坑，不是理论担心。
 * ==========================================================================*/
#include "tndrt.h"

#define TREE_MAX 48

static int gDirs = 0, gFiles = 0;
static int gMaxDepth = 4;

typedef struct { char name[TND_NAME_MAX]; tnd_u32 attr; tnd_u32 size; } ENTRY;

static void walk(const char *dir, const char *prefix, int depth) {
    ENTRY list[TREE_MAX];
    int n = 0, i;
    char pat[TND_PATH_MAX];
    TND_FIND f;
    int fh;

    if (depth > gMaxDepth) return;

    tnd_strncpy(pat, dir, sizeof(pat));
    if (tnd_strlen(pat) && pat[tnd_strlen(pat) - 1] != '\\')
        tnd_strcat(pat, "\\", sizeof(pat));
    tnd_strcat(pat, "*", sizeof(pat));

    fh = tnd_findfirst(pat, &f);
    if (fh < 0) return;

    /* 第一步：全部收下来 */
    do {
        if (n >= TREE_MAX) break;
        if (f.Name[0] == '.') continue;
        tnd_strncpy(list[n].name, f.Name, TND_NAME_MAX);
        list[n].attr = f.Attr;
        list[n].size = f.Size;
        n++;
    } while (tnd_findnext(fh, &f) == 0);
    tnd_findclose(fh);

    /* 第二步：打印并递归 */
    for (i = 0; i < n; i++) {
        if (list[i].attr & TND_ATTR_DIR) {
            char sub[TND_PATH_MAX];
            tnd_printf("%s+- %s\\\n", prefix, list[i].name);
            gDirs++;
            tnd_strncpy(sub, dir, sizeof(sub));
            if (tnd_strlen(sub) && sub[tnd_strlen(sub) - 1] != '\\')
                tnd_strcat(sub, "\\", sizeof(sub));
            tnd_strcat(sub, list[i].name, sizeof(sub));
            walk(sub, "   ", depth + 1);
        } else {
            tnd_printf("%s+- %s\n", prefix, list[i].name);
            gFiles++;
        }
    }
}

int tnx_main(void) {
    const char *start = ".";
    if (tnd_argc() > 1) start = tnd_argv(1);

    tnd_printf("\n Directory tree: %s\n\n", start);
    walk(start, "  ", 0);
    tnd_printf("\n  %d dir(s), %d file(s)\n", gDirs, gFiles);
    return 0;
}