/* ============================================================================
 * TREE -- 以树形显示目录结构
 *
 * 递归的关键：api_findfirst 会把模式拆成"目录部分 + 名字部分"，
 * 所以传 "SUB\*" 就能遍历子目录，不必先 chdir。
 *
 * 但**必须先收集完这一层再递归** —— 边遍历边开新的目录句柄，
 * 底层 UEFI 的目录读取位置会互相干扰。这是踩过的坑，不是理论担心。
 *
 * 画线规则（经典的 +- / \- 画法）：
 *   不是最后一个 -> "+-" ，并且给子层的前缀加 "|  "
 *   是最后一个   -> "\-" ，并且给子层的前缀加 "   "
 *
 * 少了这个，子目录里的文件和父目录的文件看起来就是同一层 ——
 * 那样输出的叫"目录列表"，不叫"目录树"。
 * ==========================================================================*/
#include "tndrt.h"

#define TREE_MAX   64
#define PFX_MAX   128

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
    if (tnd_strlen(pat) && pat[tnd_strlen(pat) - 1] != '\\') tnd_strcat(pat, "\\", sizeof(pat));
    tnd_strcat(pat, "*", sizeof(pat));

    fh = tnd_findfirst(pat, &f);
    if (fh < 0) return;

    /* 第一步：全收下来。边遍历边递归会让目录句柄的位置互相干扰。 */
    do {
        if (n >= TREE_MAX) break;
        if (f.Name[0] == '.') continue;
        tnd_strncpy(list[n].name, f.Name, TND_NAME_MAX);
        list[n].attr = f.Attr;
        list[n].size = f.Size;
        n++;
    } while (tnd_findnext(fh, &f) == 0);
    tnd_findclose(fh);

    /* 第二步：画 */
    for (i = 0; i < n; i++) {
        int last = (i == n - 1);
        int isdir = (list[i].attr & TND_ATTR_DIR) != 0;

        tnd_printf("%s%s %s", prefix, last ? "\\-" : "+-", list[i].name);

        if (isdir) {
            char sub[TND_PATH_MAX], childpfx[PFX_MAX];
            gDirs++;
            tnd_puts("\\\n");
            if (depth + 1 > gMaxDepth) continue;

            tnd_strncpy(sub, dir, sizeof(sub));
            if (tnd_strlen(sub) && sub[tnd_strlen(sub) - 1] != '\\') tnd_strcat(sub, "\\", sizeof(sub));
            tnd_strcat(sub, list[i].name, sizeof(sub));

            tnd_strncpy(childpfx, prefix, sizeof(childpfx));
            tnd_strcat(childpfx, last ? "   " : "|  ", sizeof(childpfx));

            walk(sub, childpfx, depth + 1);
            continue;
        }

        tnd_printf("   (%u)\n", list[i].size);
        gFiles++;
    }
}

int tnx_main(void) {
    const char *start = ".";
    if (tnd_argc() > 1) start = tnd_argv(1);
    if (tnd_argc() > 2) gMaxDepth = tnd_atoi(tnd_argv(2));

    tnd_printf("\n Directory tree: %s\n\n", start);
    walk(start, "  ", 0);
    tnd_printf("\n  %d dir(s), %d file(s)\n", gDirs, gFiles);
    return 0;
}
