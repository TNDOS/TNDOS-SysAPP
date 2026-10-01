# TNDOS-SysAPP

TNDDOS 的**外部命令**。每个都是一个 TNX 程序 —— 用 TNDOS-SDK 写的，跑在 TNDDOS 上。

这跟 DOS 的分法是同一个：内部命令（DIR、CD、COPY、SET…）住在 Shell 里，因为它们要
Shell 的状态；外部命令是独立程序，因为它们自包含。

| 命令 | 说明 |
|---|---|
| `EXECOM` | 不是工具，是**拦截器**：解释为什么一个 .EXE/.COM 跑不了 |
| `MORE` | 分屏显示文件 |
| `FIND` | 在文件里找文本（`/I` 忽略大小写，`/C` 只计数） |
| `FC` | 逐行或逐字节比较两个文件 |
| `TREE` | 树形显示目录结构 |
| `ATTRIB` | 显示文件属性（v1 只读） |
| `EDIT` | **全屏文本编辑器** |

当前版本 **v1.0**。

---

## 构建

三个环境变量决定工具链在哪，脚本里不写死任何机器相关的路径：

| 变量 | 指向 |
|---|---|
| `TNDDOS_LLVM_BIN` | 含 `clang.exe` 和 `ld.lld.exe` 的目录 |
| `TNDDOS_SDK` | TNDOS-SDK 仓库根目录 |
| `TNDDOS_TOOLKIT` | TNDOS-ToolsKit 仓库根目录（提供 `tnxpack.ps1`） |

```powershell
.\tools\build.ps1              构建全部
.\tools\build.ps1 MORE EDIT    只构建指定的几个
```

每个 `src/<name>/<name>.c` 产出一个 `bin/<NAME>.TNX`。
SysCore 构建时会自动把 `bin\*.TNX` 部署进 EFI System Partition
（找 `TNDDOS_SYSAPP`，找不到就在 `repos/TNDOS-SysAPP` 猜一下）。

---

## EXECOM 值得单独说

它可能是这个仓库里最有趣的一个。

Shell 找程序的顺序是 `.TNX` -> `.EXE` -> `.COM`。你敲 `HELLO`，
磁盘上只有 `HELLO.EXE` 时，**你不会得到一句 Bad command** ——
Shell 把文件交给 EXECOM，EXECOM 告诉你它到底是什么。

```
C:\>DOSDEMO.EXE

  This is a 16-bit DOS program (MZ).

  The CPU is in long mode. Real-mode 8086 code cannot execute
  here -- not unsupported, but physically impossible: there is no
  16-bit code segment, and no BIOS to call even if there were.

  TNDDOS does not emulate an 8086, and never will.
```

**实现上的坑**：MZ 和 PE 的前两个字节**都是 MZ**。PE 文件开头也有一份 DOS 存根，
真正的 PE 签名由偏移 `0x3C` 的 `e_lfanew` 指向。光看开头分不出来，必须跟过去看。

EXECOM 不在时 Shell 有自己的兜底消息 —— 删掉它不会让你变成哑巴，只是少一层细节。

---

## EDIT 是 SDK 的验收测试

它不是"顺便做的工具"。一个全屏编辑器碰得到 API 的每一寸：
读键盘（**还要方向键的扫描码** —— 方向键的 UnicodeChar 是 0，只看它等于什么都没读到）、
定位光标、清屏、读写文件、装得下一块文本缓冲。

**能只用 SDK 在用户态写出它，就说明这个 API 完整到可以叫操作系统了。**

事实上它是这么写出来的：一开始只有 `puts/alloc/free`，一上手 EDIT 立刻暴露出
没有 `cls`、没有 `gotoxy`、读键拿不到扫描码 —— 于是 API 补到 v2.1。

按键：方向键移动，`Enter` 断行，`Backspace`/`Del` 删除，
`F2` 保存，`ESC` 退出（有未保存改动会拦一下）。

v1 的取舍写在这里，不假装支持：行式缓冲 256x128、没有折行、没有搜索、没有撤销。

---

## 写新命令

```c
#include "tndrt.h"

int tnx_main(void) {
    tnd_printf("hello from %s\n", tnd_argv(0));
    return 0;
}
```

新建 `src/<name>/<name>.c`，然后 `.\tools\build.ps1 <name>`。
`tndrt.h` 里有什么，看 TNDOS-SDK 的 README。

---

## 环境变量汇总

```
setx TNDDOS_LLVM_BIN "D:\LLVM\bin"
setx TNDDOS_SDK      "D:\TNDOS-SDK"
setx TNDDOS_TOOLKIT  "D:\TNDOS-ToolsKit"
```

---

## 相关

- **TNDOS-SysCore** —— 操作系统本体（TNX 加载器、Shell、内核）
- **TNDOS-SDK** —— 写这些命令用的头文件、运行时、链接脚本
- **TNDOS-ToolsKit** —— 宿主机工具（tnxpack / tnxdump / mkfat）
