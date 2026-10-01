# ============================================================================
# build.ps1 -- 构建 TNDOS 外部命令（TNX 程序）
#
#   .\tools\build.ps1                构建全部
#   .\tools\build.ps1 MORE EDIT      只构建指定的几个
#
# 每个 src/<name>/<name>.c 产出一个 bin/<NAME>.TNX。
# 三个环境变量决定工具链在哪：TNDDOS_LLVM_BIN / TNDDOS_SDK / TNDDOS_TOOLKIT
# ============================================================================
param(
    [Parameter(Position=0)][string[]]$Name,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
. (Join-Path $PSScriptRoot 'env.ps1')

function Invoke-Native([string]$Exe, [string[]]$Arguments) {
    # 编译器的 warning 走 stderr；不隔离的话 Stop 模式会因为它中止整个构建
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Exe @Arguments
    $script:NativeExit = $LASTEXITCODE
    $ErrorActionPreference = $prev
}

$bin = Join-Path $CMDS_ROOT 'bin'
$obj = Join-Path $CMDS_ROOT 'obj'
New-Item -ItemType Directory -Force -Path $bin, $obj | Out-Null

# 运行时先编一次，所有命令共用
$rt = Join-Path $obj 'tndrt.o'
Invoke-Native $CLANG @('-target','x86_64-unknown-none','-ffreestanding','-fno-builtin','-fno-stack-protector',
                       '-mno-red-zone','-nostdlib','-Wall','-I',$SDK_INC,'-I',$SDK_LIB,
                       '-c',(Join-Path $SDK_LIB 'tndrt.c'),'-o',$rt)
if ($script:NativeExit -ne 0) { throw 'tndrt.c 编译失败' }

$all = Get-ChildItem (Join-Path $CMDS_ROOT 'src') -Directory | ForEach-Object { $_.Name }
if ($Name -and $Name.Count) { $targets = $Name } else { $targets = $all }

$ok = 0; $fail = 0
foreach ($t in $targets) {
    $src = Join-Path $CMDS_ROOT ("src\" + $t + "\" + $t + ".c")
    if (-not (Test-Path $src)) { Write-Host ("  [skip] " + $t + " (no source)"); continue }
    $o   = Join-Path $obj ($t + '.o')
    $elf = Join-Path $obj ($t + '.elf')
    $out = Join-Path $bin ($t.ToUpper() + '.TNX')

    Invoke-Native $CLANG @('-target','x86_64-unknown-none','-ffreestanding','-fno-builtin','-fno-stack-protector',
                           '-mno-red-zone','-nostdlib','-Wall','-I',$SDK_INC,'-I',$SDK_LIB,
                           '-c',$src,'-o',$o)
    if ($script:NativeExit -ne 0) { Write-Host ("  [FAIL] " + $t); $fail++; continue }

    Invoke-Native $LLD @('-m','elf_x86_64','-T',(Join-Path $SDK_LD 'tnx.ld'),'-o',$elf,$o,$rt)
    if ($script:NativeExit -ne 0) { Write-Host ("  [FAIL] link " + $t); $fail++; continue }

    & $TNXPACK -In $elf -Out $out -Quiet
    if ($LASTEXITCODE -ne 0) { Write-Host ("  [FAIL] pack " + $t); $fail++; continue }

    if (-not $Quiet) { Write-Host ("  [tnx] " + $t.ToUpper() + ".TNX   " + (Get-Item $out).Length + " bytes") }
    $ok++
}

if ($ok -gt 0) {
    $tail = if ($fail) { ", " + $fail + " FAILED" } else { "" }
    Write-Host ("  " + $ok + " command(s) built" + $tail)
}
if ($fail) { exit 1 } else { exit 0 }
