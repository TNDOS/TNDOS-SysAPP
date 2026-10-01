/* ============================================================================
 * EXECOM -- 解释"为什么这个 DOS/Windows 程序跑不了"
 *
 * 它不是工具，是**拦截器**。Shell 找不到 <name>.TNX 时会去找 <name>.EXE /
 * <name>.COM，找到就把路径交给这里。
 *
 * 关键实现细节：**MZ 和 PE 的前两个字节都是 MZ**。
 * PE 文件开头也有一份 DOS 存根（为了兼容远古工具），真正的 PE 签名由
 * 偏移 0x3C 的 e_lfanew 指向。所以光看开头分不出来，必须跟过去看。
 * ==========================================================================*/
#include "tndrt.h"

static const char *leaf(const char *p) {
    const char *b = p;
    for (; p && *p; p++) if (*p == '\\' || *p == '/' || *p == ':') b = p + 1;
    return b ? b : "";
}

int tnx_main(void) {
    const char *path;
    char hdr[128];
    int fd;
    tnd_i64 n;

    tnd_puts("\n");

    if (tnd_argc() < 2) {
        tnd_puts("  EXECOM -- explains why a DOS/Windows executable cannot run here.\n\n");
        tnd_puts("  Normally you never run this by hand. Just type the program name,\n");
        tnd_puts("  e.g.  C:\\>HELLO   while HELLO.EXE exists, and the shell hands\n");
        tnd_puts("  the file to EXECOM for a diagnosis.\n");
        return 1;
    }

    path = tnd_argv(1);
    tnd_printf("  %s\n\n", path);

    fd = tnd_open(path, TND_O_RDONLY);
    if (fd < 0) { tnd_printf("  cannot open %s\n", path); return 1; }

    n = tnd_read(fd, hdr, sizeof(hdr));

    /* --- MZ：可能是 16 位 DOS 程序，也可能是 PE --- */
    if (n >= 64 && (unsigned char)hdr[0] == 'M' && (unsigned char)hdr[1] == 'Z') {
        unsigned int lfanew = (unsigned char)hdr[0x3C] | ((unsigned char)hdr[0x3D] << 8) |
                              ((unsigned char)hdr[0x3E] << 16) | ((unsigned char)hdr[0x3F] << 24);
        unsigned char sig[4];
        sig[0] = sig[1] = sig[2] = sig[3] = 0;
        tnd_seek(fd, (tnd_i64)lfanew, TND_SEEK_SET);
        tnd_read(fd, sig, 4);
        tnd_close(fd);

        if (sig[0] == 'P' && sig[1] == 'E' && sig[2] == 0 && sig[3] == 0) {
            tnd_puts("  This is a PE image (Windows / UEFI application).\n\n");
            tnd_puts("  TNDDOS does not load PE images. Its own format is TNX.\n");
            tnd_puts("  That is the design, not a gap: a PE loader is eight hundred\n");
            tnd_puts("  lines of edge cases, TNX is a memcpy and a jump.\n");
        } else {
            tnd_puts("  This is a 16-bit DOS program (MZ).\n\n");
            tnd_puts("  The CPU is in long mode. Real-mode 8086 code cannot execute\n");
            tnd_puts("  here -- not unsupported, but physically impossible: there is no\n");
            tnd_puts("  16-bit code segment, and no BIOS to call even if there were.\n\n");
            tnd_puts("  TNDDOS does not emulate an 8086, and never will.\n");
        }
        return 2;
    }

    /* --- COM：没有文件头，只能靠扩展名 --- */
    {
        const char *nm = leaf(path);
        int len = (int)tnd_strlen(nm);
        if (len >= 4 && tnd_stricmp(nm + len - 4, ".COM") == 0) {
            tnd_close(fd);
            tnd_puts("  This is a 16-bit COM program.\n\n");
            tnd_puts("  COM files carry no header at all -- just raw real-mode code\n");
            tnd_puts("  loaded at 0x100. There is nothing here to translate.\n");
            tnd_puts("  It cannot run in long mode.\n");
            return 2;
        }
    }

    if (n >= 4 && (unsigned char)hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
        tnd_close(fd);
        tnd_puts("  This is an ELF binary (Linux / Unix).\n");
        tnd_puts("  Wrong operating system, and the ABI it expects is not the one you get.\n");
        return 2;
    }
    if (n >= 4 && hdr[0] == 'T' && hdr[1] == 'N' && hdr[2] == 'X' && (unsigned char)hdr[3] == 0x1A) {
        tnd_close(fd);
        tnd_puts("  This is a TNX program.\n");
        tnd_puts("  The shell should have run it directly -- that is a shell bug,\n");
        tnd_puts("  not a problem with this file.\n");
        return 3;
    }

    tnd_close(fd);
    tnd_puts("  Unknown file format. TNDDOS cannot run this.\n\n");
    tnd_puts("  first bytes:");
    for (int i = 0; i < 16 && i < (int)n; i++) tnd_printf(" %x", (unsigned char)hdr[i]);
    tnd_puts("\n");
    return 2;
}
