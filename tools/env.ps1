# TNDOS-SysAPP 的工具链定位（dot-source 引入）
# 和 TNDOS-SDK 一个规矩：脚本里不写死机器相关的路径。
#
#   TNDDOS_LLVM_BIN   含 clang.exe 与 ld.lld.exe 的目录
#   TNDDOS_SDK        TNDOS-SDK 仓库根目录
#   TNDDOS_TOOLKIT    TNDOS-ToolsKit 仓库根目录（提供 tnxpack.ps1）

$script:NL = [char]10
$script:Q  = [char]34

function __HowTo([string]$Name, [string]$Example) {
    return ("请设置环境变量 " + $Name + "，然后重开一个终端：" + $script:NL +
            "    setx " + $Name + " " + $script:Q + $Example + $script:Q)
}
function __EnvDir([string]$Name) {
    $v = [Environment]::GetEnvironmentVariable($Name)
    if (-not $v) { return $null }
    if (-not (Test-Path -LiteralPath $v)) { throw ("环境变量 " + $Name + " 指向的路径不存在：" + $script:NL + "    " + $v) }
    return (Get-Item -LiteralPath $v).FullName
}

$CMDS_ROOT = Split-Path -Parent $PSScriptRoot

$llvm = __EnvDir 'TNDDOS_LLVM_BIN'
if ($llvm) { $CLANG = Join-Path $llvm 'clang.exe'; $LLD = Join-Path $llvm 'ld.lld.exe' }
else {
    $c = Get-Command clang.exe -ErrorAction SilentlyContinue
    if (-not $c) { throw ("找不到 clang.exe。" + $script:NL + (__HowTo 'TNDDOS_LLVM_BIN' 'D:\LLVM\bin')) }
    $CLANG = $c.Source; $LLD = Join-Path (Split-Path -Parent $CLANG) 'ld.lld.exe'
}

$SDK = __EnvDir 'TNDDOS_SDK'
if (-not $SDK) { throw ("找不到 TNDOS-SDK（TNDDOS_SDK 未设置）。" + $script:NL + (__HowTo 'TNDDOS_SDK' 'D:\TNDOS-SDK')) }
$SDK_INC = Join-Path $SDK 'include'
$SDK_LIB = Join-Path $SDK 'lib'
$SDK_LD  = Join-Path $SDK 'linker'

$TOOLKIT = __EnvDir 'TNDDOS_TOOLKIT'
if (-not $TOOLKIT) { throw ("找不到 TNDOS-ToolsKit（TNDDOS_TOOLKIT 未设置）。" + $script:NL + (__HowTo 'TNDDOS_TOOLKIT' 'D:\TNDOS-ToolsKit')) }
$TNXPACK = Join-Path $TOOLKIT 'tnxpack.ps1'
