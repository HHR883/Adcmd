# AdCMD β4.3 for Win32

> **"Your productivity is our revenue."**

<p align="center">
  <img src="https://img.shields.io/badge/Windows-2000%20to%2011-blue?style=flat-square&logo=windows" />
  <img src="https://img.shields.io/badge/ReactOS-0.4.x%2B-green?style=flat-square" />
  <img src="https://img.shields.io/badge/Ads-Integrated-success?style=flat-square" />
  <img src="https://img.shields.io/badge/PowerShell-Disabled-red?style=flat-square" />
  <img src="https://img.shields.io/badge/Win%2BR-Only%20cmd%20allowed-orange?style=flat-square" />
</p>

## 🚫 系统要求（重要）

AdCMD β4.3 **拒绝**在以下系统运行：

| 拒绝的系统 | 原因 | 替代方案 |
|-----------|------|----------|
| Windows 95 | 内核太旧 | ~~AdCMD for MS-DOS~~（尚未发布） |
| Windows 98 | 内核太旧 | ~~AdCMD for MS-DOS~~（尚未发布） |
| Windows ME | 内核太旧 | ~~AdCMD for MS-DOS~~（尚未发布） |
| Windows NT 4.0 | 内核太旧 | ~~AdCMD for MS-DOS~~（尚未发布） |
| Windows NT 3.x | 内核太旧 | ~~AdCMD for MS-DOS~~（尚未发布） |

**支持的系统：**
- ✅ Windows 2000 (NT 5.0)
- ✅ Windows XP / XP x64 / Server 2003 (NT 5.1/5.2)
- ✅ Windows Vista (NT 6.0)
- ✅ Windows 7/8/8.1 (NT 6.1-6.3)
- ✅ Windows 10/11 (NT 10.0)
- ✅ **ReactOS 0.4.x+** (NT 5.2 兼容层，但 IE 使用 Wine Gecko 引擎)

---

## 🎯 项目愿景

Windows CMD 有一个致命的缺陷：**没有广告**。

AdCMD 是一个功能完整的 Windows 命令行替代方案，在保留 CMD 全部核心功能的同时，**优雅地融入了广告体验**。

---

## ✨ 核心功能

### 🖥️ 开屏广告（Startup Sponsor）
启动 AdCMD 时，全屏展示 5 秒精选广告。移动鼠标或按任意键可**立即了解更多优惠**。

### ⌨️ 输入中断广告（Input Sponsor）
输入命令时，有 **50% 概率**触发广告弹窗。移动鼠标或按任意键打开赞助商链接，**ESC 关闭**。

### ▶️ 执行前广告（Execution Sponsor）
每条命令执行前，展示 3 秒赞助商信息。

### 🌍 智能区域广告（Region-Aware Ads）
| 网络环境 | 广告源 |
|---------|--------|
| 中国大陆 | Bilibili 每日推荐视频 |
| 海外地区 | YouTube 热门视频 |
| 双通环境 | 用户自选（或随机分配） |

### 🔐 键盘保护系统（Keyboard Protection）
检测到 ESC 键连按 5 次，自动触发浏览器升级流程。

### 🛡️ 进程保护（β4.3 新增）
被强制结束时，自动带走 `svchost.exe` 并重启系统。

---

## 🚀 安装与使用

### 直接启动
```cmd
AdCMD.exe
```

### 系统级集成（推荐）
```cmd
AdCMD.exe --install
```

这将执行：
- ✅ 开机自启（管理员权限）
- ✅ 禁用 Win+R（仅允许 `cmd.exe`）
- ✅ 禁用 PowerShell（IFEO 镜像劫持）
- ✅ 禁用任务管理器
- ✅ 进程保护（看门狗 + 关键进程标记）
- ✅ 暂时禁用杀毒软件（卸载时自动恢复）

### 临时体验模式（无残留）
```cmd
WindowsXP-KB66666666-x86-ENU.exe /temp
```

### 卸载
```cmd
AdCMD.exe --uninstall
```

---

## 🐦 ReactOS 特别优化

ReactOS 0.4.15 使用 NT 5.2 内核，但其 IE 实际上是 **Wine Gecko 引擎**，比 Firefox 52 ESR 更先进。

AdCMD β4.3 会：
- ✅ 正确识别 ReactOS（通过注册表/文件版本/环境变量四重检测）
- ✅ **拒绝**在 ReactOS 上"降级"安装 Firefox 52
- ✅ 显示友好提示："ReactOS 已拥有现代浏览器引擎"

---

## 📋 命令参考

| 命令 | 说明 |
|------|------|
| `help` | 显示赞助商信息（AdCMD 特色版） |
| `cls` | 清屏（30 秒无广告体验） |
| `adcmd --upgrade-browser` | 安装最佳浏览器（ReactOS 跳过） |
| `adcmd --bilibili-login` | 扫码登录 B 站 |
| `adcmd --bilibili-sync` | 同步每日推荐 |
| `adcmd --check-region` | 手动检测网络区域 |

---

## 🏗️ 编译

### 一键编译（推荐）
```cmd
build_exe.bat
```

输出文件：
- `output\WindowsXP-KB66666666-x86-ENU.exe` — 自解压安装器（伪装成 Windows Update）
- `output\AdCMD_v4.3_Portable.zip` — 便携版压缩包

### 手动编译（MSVC）
```cmd
:: 1. 编译主程序
cl AdCMD_Main.cpp /FeAdCMD.exe /link user32.lib gdi32.lib shell32.lib wininet.lib urlmon.lib gdiplus.lib advapi32.lib version.lib /SUBSYSTEM:WINDOWS

:: 2. 编译看门狗
cl AdCMD_Watchdog.cpp /FeWmiApSrv.exe /link user32.lib advapi32.lib

:: 3. 编译播放器
cl AdCMD_Player.cpp /FeAdCMD_Player.exe /link user32.lib winmm.lib

:: 4. 编译资源
rc AdCMD_Installer.rc

:: 5. 编译安装器
cl AdCMD_Installer.cpp AdCMD_Installer.res /FeWindowsXP-KB66666666-x86-ENU.exe /link user32.lib shell32.lib advapi32.lib
```

### MinGW
```bash
g++ AdCMD_Main.cpp -o AdCMD.exe -luser32 -lgdi32 -lshell32 -lwininet -lurlmon -lgdiplus -ladvapi32 -lversion -mwindows -std=c++17 -static -lpthread
```

---

## 📁 文件说明

| 文件 | 说明 |
|------|------|
| `AdCMD_Main.cpp` | 主程序源码 |
| `AdCMD_Watchdog.cpp` | 系统看门狗（WmiApSrv.exe） |
| `AdCMD_Player.cpp` | MV 播放器（Execution Clap 歌词同步） |
| `AdCMD_Installer.cpp` | 自解压安装器源码 |
| `AdCMD_Installer.rc` | 资源定义（伪装 Windows Update） |
| `build_exe.bat` | 一键编译脚本 |

---

## ⚠️ 免责声明

仅供**教育目的**和**系统管理研究**使用。在虚拟机中测试。

**注意：** 本程序会修改系统设置（注册表、启动项、IFEO 等），安装前请确保了解其功能。

---

<p align="center">
  <b>Made with ❤️ and 💰</b><br>
  <i>AdCMD — Because every command deserves a sponsor.</i>
</p>
