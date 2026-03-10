// AVIF-Master - Win32 AVIF Batch Converter
// Unicode Win32 API, Virtual ListView, Master-Slave IPC

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <sstream>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")

// ─── Constants ──────────────────────────────────────────────────────────────
#define WM_APP_ADD_FILE      (WM_APP + 1)
#define WM_APP_UPDATE_LIST   (WM_APP + 2)
#define WM_APP_CONV_DONE     (WM_APP + 3)
#define MY_COPYDATA_ID       1

#define RIGHT_PANEL_W        250
#define BOTTOM_BAR_H         50
#define TAB_H                24
#define STATUSBAR_H          22

static const wchar_t CLASS_NAME[]  = L"AVIFMasterWindowClass";
static const wchar_t WINDOW_NAME[] = L"AVIF-Master";
static const wchar_t MUTEX_NAME[]  = L"AVIFMasterGlobalMutex_v1";
static const wchar_t ARGS_PREFIX[] = L"AVIFMaster_args_";

// ─── Data Structures ─────────────────────────────────────────────────────────
enum class ConvStatus { Waiting, Converting, Done, Failed };

struct FileItem {
    std::wstring fullPath;
    std::wstring name;
    std::wstring type;
    std::wstring originalSize;
    ConvStatus   status      = ConvStatus::Waiting;
    std::wstring statusText  = L"Waiting";
    std::wstring resultSize;
    std::wstring ratio;
    LONGLONG     originalBytes = 0;
    LONGLONG     resultBytes   = 0;
};

struct Settings {
    bool  cpuGpu       = false;   // false=CPU Only, true=CPU+GPU
    bool  gpuCache     = false;
    int   engine       = 0;       // 0=avifenc, 1=ffmpeg, 2=av1an
    int   jobs         = 4;
    int   threadsPerJob= 8;
    bool  simd         = true;
    int   outMode      = 0;       // 0=same folder, 1=custom, 2=subdir
    std::wstring outPath;
    bool  skipExisting = true;
    bool  denoise      = false;
    bool  resize       = false;
    int   resizeW      = 1920;
    int   resizeH      = 1080;
    bool  autoClose    = false;
};

// ─── Globals ──────────────────────────────────────────────────────────────────
HINSTANCE     g_hInst;
HANDLE        g_hMutex;
HWND          g_hMainWnd;
HWND          g_hListView;
HWND          g_hStartBtn;
HWND          g_hTab;
HWND          g_hPerfPanel;
HWND          g_hFileMgmtPanel;
HWND          g_hStatusBar;
HWND          g_hProgressBar;
HWND          g_hBottomBar;

std::vector<FileItem> g_items;
std::mutex            g_itemsMutex;
std::atomic<bool>     g_converting(false);
Settings              g_settings;

// Per-tab child windows (Performance)
HWND g_hRadioCpuOnly, g_hRadioCpuGpu, g_hCheckGpuCache;
HWND g_hRadioAvifenc, g_hRadioFfmpeg, g_hRadioAv1an;
HWND g_hEditJobs, g_hEditThreads, g_hCheckSimd;

// Per-tab child windows (File Management)
HWND g_hRadioOutSame, g_hRadioOutCustom, g_hRadioOutSubdir;
HWND g_hEditOutPath, g_hBtnBrowse;
HWND g_hCheckSkip, g_hCheckDenoise, g_hCheckResize;
HWND g_hEditResizeW, g_hEditResizeH;
HWND g_hBtnSaveProfile, g_hBtnLoadProfile;
HWND g_hCheckAutoClose;

// ─── Forward Declarations ────────────────────────────────────────────────────
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void CreateMainUI(HWND hwnd);
void CreatePerfPanel(HWND hParent);
void CreateFileMgmtPanel(HWND hParent);
void InitListView(HWND hLV);
void AddFile(const wchar_t* path);
void StartConversion();
void ConversionWorker();
void ShowReportDialog(HWND hParent, LONGLONG totalOrig, LONGLONG totalResult,
                      int success, int failed, double elapsed);
void LayoutWindows(HWND hwnd);
std::wstring FormatBytes(LONGLONG bytes);
std::wstring GetFileExt(const std::wstring& path);

// ─── Helpers ─────────────────────────────────────────────────────────────────
std::wstring FormatBytes(LONGLONG b) {
    if (b <= 0) return L"0 B";
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB"};
    int u = 0;
    double v = (double)b;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; ++u; }
    wchar_t buf[64];
    if (u == 0) swprintf_s(buf, L"%.0f %s", v, units[u]);
    else        swprintf_s(buf, L"%.1f %s", v, units[u]);
    return buf;
}

std::wstring GetFileExt(const std::wstring& path) {
    size_t dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return L"FILE";
    std::wstring ext = path.substr(dot + 1);
    for (auto& c : ext) c = towupper(c);
    return ext;
}

std::wstring GetFileName(const std::wstring& path) {
    size_t sl = path.rfind(L'\\');
    if (sl == std::wstring::npos) sl = path.rfind(L'/');
    if (sl == std::wstring::npos) return path;
    return path.substr(sl + 1);
}

// ─── AddFile (thread-safe) ───────────────────────────────────────────────────
void AddFile(const wchar_t* path) {
    if (!path || !path[0]) return;

    FileItem item;
    item.fullPath = path;
    item.name     = GetFileName(item.fullPath);
    item.type     = GetFileExt(item.fullPath);

    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER sz;
        sz.HighPart = fad.nFileSizeHigh;
        sz.LowPart  = fad.nFileSizeLow;
        item.originalBytes = (LONGLONG)sz.QuadPart;
        item.originalSize  = FormatBytes(item.originalBytes);
    } else {
        item.originalSize = L"N/A";
    }

    item.status     = ConvStatus::Waiting;
    item.statusText = L"Waiting";

    std::lock_guard<std::mutex> lk(g_itemsMutex);
    g_items.push_back(std::move(item));
}

// ─── WinMain ─────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInst;

    INITCOMMONCONTROLSEX icex = { sizeof(icex),
        ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_PROGRESS_CLASS |
        ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    // ── Single-instance / Master-Slave ──
    g_hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    bool isMaster = (g_hMutex && GetLastError() != ERROR_ALREADY_EXISTS);

    if (!isMaster) {
        // Slave: send args to master via WM_COPYDATA
        HWND hMaster = FindWindowW(CLASS_NAME, NULL);
        if (hMaster) {
            int argc;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (argv) {
                for (int i = 1; i < argc; ++i) {
                    COPYDATASTRUCT cds = {};
                    cds.dwData  = MY_COPYDATA_ID;
                    cds.cbData  = (DWORD)((wcslen(argv[i]) + 1) * sizeof(wchar_t));
                    cds.lpData  = argv[i];
                    SendMessageW(hMaster, WM_COPYDATA, 0, (LPARAM)&cds);
                }
                LocalFree(argv);
            }
        }
        if (g_hMutex) CloseHandle(g_hMutex);
        return 0;
    }

    // ── Register Window Class ──
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(MAIN_ICON));
    wc.hIconSm       = wc.hIcon;
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        CLASS_NAME, WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 620,
        NULL, NULL, hInst, NULL);

    if (!g_hMainWnd) { CloseHandle(g_hMutex); return 1; }

    // ── Collect command-line args ──
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startWithFiles = (argc > 1);
    if (argv) {
        for (int i = 1; i < argc; ++i) AddFile(argv[i]);
        LocalFree(argv);
    }

    ListView_SetItemCountEx(g_hListView, (int)g_items.size(), LVSICF_NOINVALIDATEALL);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    if (startWithFiles)
        PostMessageW(g_hMainWnd, WM_COMMAND, MAKEWPARAM(IDC_START_BUTTON, BN_CLICKED), 0);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(g_hMutex);
    return (int)msg.wParam;
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void LayoutWindows(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    int listW = W - RIGHT_PANEL_W;
    int listH = H - BOTTOM_BAR_H - STATUSBAR_H;

    // ListView
    if (g_hListView)
        SetWindowPos(g_hListView, NULL, 0, 0, listW, listH, SWP_NOZORDER);

    // Right panel (tab + content)
    if (g_hTab)
        SetWindowPos(g_hTab, NULL, listW, 0, RIGHT_PANEL_W, H - STATUSBAR_H, SWP_NOZORDER);

    // Tab panel contents (below tab header)
    int panelY = TAB_H + 2;
    int panelH = H - STATUSBAR_H - panelY;
    if (g_hPerfPanel)
        SetWindowPos(g_hPerfPanel, NULL, listW, panelY, RIGHT_PANEL_W, panelH, SWP_NOZORDER);
    if (g_hFileMgmtPanel)
        SetWindowPos(g_hFileMgmtPanel, NULL, listW, panelY, RIGHT_PANEL_W, panelH, SWP_NOZORDER);

    // Bottom bar
    if (g_hBottomBar)
        SetWindowPos(g_hBottomBar, NULL, 0, H - BOTTOM_BAR_H - STATUSBAR_H,
                     listW, BOTTOM_BAR_H, SWP_NOZORDER);

    // Start button (inside bottom bar — reposition relative to bar)
    if (g_hStartBtn)
        SetWindowPos(g_hStartBtn, NULL, 8, 8, listW - 16, BOTTOM_BAR_H - 16, SWP_NOZORDER);

    // Progress bar
    if (g_hProgressBar)
        SetWindowPos(g_hProgressBar, NULL, 0, H - STATUSBAR_H - BOTTOM_BAR_H,
                     listW, 4, SWP_NOZORDER);

    // Status bar spans full width at bottom
    if (g_hStatusBar)
        SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
}

// ─── Create Main UI ──────────────────────────────────────────────────────────
void CreateMainUI(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int listW = W - RIGHT_PANEL_W;

    // ListView
    g_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
        0, 0, listW, H - BOTTOM_BAR_H - STATUSBAR_H,
        hwnd, (HMENU)IDC_MAIN_LISTVIEW, g_hInst, NULL);

    ListView_SetExtendedListViewStyle(g_hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
    InitListView(g_hListView);

    // Bottom bar background (static)
    g_hBottomBar = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, H - BOTTOM_BAR_H - STATUSBAR_H, listW, BOTTOM_BAR_H,
        hwnd, NULL, g_hInst, NULL);

    // Start button (child of hwnd, positioned over bottom bar)
    g_hStartBtn = CreateWindowExW(0, L"BUTTON", L"Start Conversion",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        8, H - BOTTOM_BAR_H - STATUSBAR_H + 8,
        listW - 16, BOTTOM_BAR_H - 16,
        hwnd, (HMENU)IDC_START_BUTTON, g_hInst, NULL);

    // Progress bar (thin bar above start button)
    g_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, H - BOTTOM_BAR_H - STATUSBAR_H - 4, listW, 4,
        hwnd, (HMENU)IDC_PROGRESS_BAR, g_hInst, NULL);
    SendMessageW(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd, (HMENU)IDC_STATUSBAR, g_hInst, NULL);
    int sbParts[] = { -1 };
    SendMessageW(g_hStatusBar, SB_SETPARTS, 1, (LPARAM)sbParts);

    // Right tab control
    g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
        listW, 0, RIGHT_PANEL_W, H - STATUSBAR_H,
        hwnd, (HMENU)IDC_MAIN_TAB, g_hInst, NULL);

    TCITEMW tie = {};
    tie.mask    = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"Performance";
    TabCtrl_InsertItem(g_hTab, 0, &tie);
    tie.pszText = (LPWSTR)L"File Mgmt";
    TabCtrl_InsertItem(g_hTab, 1, &tie);

    // Panel containers (children of hwnd, overlay tab content area)
    int panelY = TAB_H + 2;
    int panelH = H - STATUSBAR_H - panelY;

    g_hPerfPanel = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        listW, panelY, RIGHT_PANEL_W, panelH,
        hwnd, NULL, g_hInst, NULL);

    g_hFileMgmtPanel = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD,   // hidden initially
        listW, panelY, RIGHT_PANEL_W, panelH,
        hwnd, NULL, g_hInst, NULL);

    CreatePerfPanel(g_hPerfPanel);
    CreateFileMgmtPanel(g_hFileMgmtPanel);
}

// ─── Helper: small label ─────────────────────────────────────────────────────
static HWND MakeLabel(HWND hParent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, hParent, NULL, g_hInst, NULL);
}
static HWND MakeCheck(HWND hParent, const wchar_t* text, int id, int x, int y, int w, int h, bool checked=false) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, w, h, hParent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (checked) SendMessageW(hw, BM_SETCHECK, BST_CHECKED, 0);
    return hw;
}
static HWND MakeRadio(HWND hParent, const wchar_t* text, int id, int x, int y, int w, int h, bool checked=false, bool first=false) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    if (first) style |= WS_GROUP;
    HWND hw = CreateWindowExW(0, L"BUTTON", text, style,
        x, y, w, h, hParent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (checked) SendMessageW(hw, BM_SETCHECK, BST_CHECKED, 0);
    return hw;
}
static HWND MakeEdit(HWND hParent, const wchar_t* text, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x, y, w, h, hParent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    return hw;
}

// ─── Create Performance Panel ────────────────────────────────────────────────
void CreatePerfPanel(HWND p) {
    int x = 8, y = 8, rw = RIGHT_PANEL_W - 16;

    MakeLabel(p, L"Hardware:", x, y, rw, 14); y += 16;
    g_hRadioCpuOnly = MakeRadio(p, L"CPU Only (Precision)", IDC_RADIO_CPU_ONLY,
                                 x, y, rw, 16, true, true); y += 18;
    g_hRadioCpuGpu  = MakeRadio(p, L"CPU + GPU (OpenCL)", IDC_RADIO_CPU_GPU,
                                 x, y, rw, 16, false); y += 18;
    g_hCheckGpuCache= MakeCheck(p, L"GPU VRAM Cache (16GB)", IDC_CHECK_GPU_CACHE,
                                 x+12, y, rw-12, 16); y += 22;

    MakeLabel(p, L"Engine:", x, y, rw, 14); y += 16;
    g_hRadioAvifenc = MakeRadio(p, L"avifenc (SVT-AV1)", IDC_RADIO_ENGINE_AVIFENC,
                                 x, y, rw, 16, true, true); y += 18;
    g_hRadioFfmpeg  = MakeRadio(p, L"FFmpeg", IDC_RADIO_ENGINE_FFMPEG,
                                 x, y, rw, 16); y += 18;
    g_hRadioAv1an   = MakeRadio(p, L"av1an", IDC_RADIO_ENGINE_AV1AN,
                                 x, y, rw, 16); y += 22;

    MakeLabel(p, L"CPU Strategy:", x, y, rw, 14); y += 16;
    MakeLabel(p, L"Concurrent Jobs:", x, y, 110, 14);
    g_hEditJobs = MakeEdit(p, L"4", IDC_EDIT_JOBS, x + 115, y - 2, 50, 18); y += 22;
    MakeLabel(p, L"Threads/Job:", x, y, 110, 14);
    g_hEditThreads = MakeEdit(p, L"8", IDC_EDIT_THREADS, x + 115, y - 2, 50, 18); y += 22;

    g_hCheckSimd = MakeCheck(p, L"Force AVX2/AVX-512 SIMD", IDC_CHECK_SIMD,
                              x, y, rw, 16, true); y += 22;
}

// ─── Create File Management Panel ────────────────────────────────────────────
void CreateFileMgmtPanel(HWND p) {
    int x = 8, y = 8, rw = RIGHT_PANEL_W - 16;

    MakeLabel(p, L"Output Path:", x, y, rw, 14); y += 16;
    g_hRadioOutSame   = MakeRadio(p, L"Same as source", IDC_RADIO_OUT_SAME,
                                   x, y, rw, 16, true, true); y += 18;
    g_hRadioOutSubdir = MakeRadio(p, L"Subdir (preserve struct)", IDC_RADIO_OUT_SUBDIR,
                                   x, y, rw, 16); y += 18;
    g_hRadioOutCustom = MakeRadio(p, L"Custom folder:", IDC_RADIO_OUT_CUSTOM,
                                   x, y, rw, 16); y += 18;
    g_hEditOutPath = MakeEdit(p, L"", IDC_EDIT_OUT_PATH, x, y, rw - 36, 18);
    g_hBtnBrowse   = CreateWindowExW(0, L"BUTTON", L"...",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      x + rw - 34, y, 34, 18,
                      p, (HMENU)IDC_BTN_BROWSE, g_hInst, NULL);
    y += 24;

    g_hCheckSkip    = MakeCheck(p, L"Skip existing .avif", IDC_CHECK_SKIP_EXIST,
                                 x, y, rw, 16, true); y += 20;
    g_hCheckDenoise = MakeCheck(p, L"GPU Denoise (OpenCL)", IDC_CHECK_DENOISE,
                                 x, y, rw, 16); y += 20;
    g_hCheckResize  = MakeCheck(p, L"Resize (LANCZOS3):", IDC_CHECK_RESIZE,
                                 x, y, rw, 16); y += 18;
    MakeLabel(p, L"W:", x, y, 16, 14);
    g_hEditResizeW = MakeEdit(p, L"1920", IDC_EDIT_RESIZE_W, x+18, y-2, 60, 18);
    MakeLabel(p, L"H:", x + 84, y, 16, 14);
    g_hEditResizeH = MakeEdit(p, L"1080", IDC_EDIT_RESIZE_H, x+100, y-2, 60, 18);
    y += 26;

    g_hCheckAutoClose = MakeCheck(p, L"Auto-close when done", IDC_AUTO_CLOSE_CHECK,
                                   x, y, rw, 16); y += 22;

    g_hBtnSaveProfile = CreateWindowExW(0, L"BUTTON", L"Save Profile",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x, y, (rw/2)-2, 22, p, (HMENU)IDC_BTN_SAVE_PROFILE, g_hInst, NULL);
    g_hBtnLoadProfile = CreateWindowExW(0, L"BUTTON", L"Load Profile",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x + rw/2 + 2, y, (rw/2)-2, 22, p, (HMENU)IDC_BTN_LOAD_PROFILE, g_hInst, NULL);
}

// ─── InitListView ─────────────────────────────────────────────────────────────
void InitListView(HWND hLV) {
    struct Col { const wchar_t* text; int w; };
    Col cols[] = {
        {L"Name",          220},
        {L"Type",           60},
        {L"Original Size",  90},
        {L"Status",        130},
        {L"Result Size",    90},
        {L"Ratio",          60},
    };
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    for (int i = 0; i < 6; ++i) {
        lvc.iSubItem = i;
        lvc.pszText  = (LPWSTR)cols[i].text;
        lvc.cx       = cols[i].w;
        ListView_InsertColumn(hLV, i, &lvc);
    }
}

// ─── Conversion Worker ────────────────────────────────────────────────────────
void ConversionWorker() {
    size_t total = 0;
    {
        std::lock_guard<std::mutex> lk(g_itemsMutex);
        total = g_items.size();
    }
    if (total == 0) {
        g_converting = false;
        PostMessageW(g_hMainWnd, WM_APP_CONV_DONE, 0, 0);
        return;
    }

    auto tStart = std::chrono::steady_clock::now();
    LONGLONG totalOrig = 0, totalResult = 0;
    int success = 0, failed = 0;

    for (size_t i = 0; i < total; ++i) {
        // Mark converting
        {
            std::lock_guard<std::mutex> lk(g_itemsMutex);
            g_items[i].status     = ConvStatus::Converting;
            g_items[i].statusText = L"Converting...";
        }
        PostMessageW(g_hMainWnd, WM_APP_UPDATE_LIST, (WPARAM)i, 0);

        // Progress bar
        int pct = (int)(((double)i / total) * 1000.0);
        PostMessageW(g_hProgressBar, PBM_SETPOS, pct, 0);

        // Simulate work (replace with real encoder call)
        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        // Simulate result
        LONGLONG origBytes = 0, resBytes = 0;
        {
            std::lock_guard<std::mutex> lk(g_itemsMutex);
            origBytes = g_items[i].originalBytes > 0
                        ? g_items[i].originalBytes
                        : 1024 * 1024;  // fallback 1 MB
            resBytes  = (LONGLONG)(origBytes * 0.35); // ~65% reduction

            g_items[i].resultBytes = resBytes;
            g_items[i].resultSize  = FormatBytes(resBytes);
            double ratio = origBytes > 0
                ? (double)(origBytes - resBytes) / origBytes * 100.0
                : 0.0;
            wchar_t rbuf[32];
            swprintf_s(rbuf, L"%.1f%%", ratio);
            g_items[i].ratio        = rbuf;
            g_items[i].status       = ConvStatus::Done;
            g_items[i].statusText   = L"Done";
            totalOrig   += origBytes;
            totalResult += resBytes;
            ++success;
        }
        PostMessageW(g_hMainWnd, WM_APP_UPDATE_LIST, (WPARAM)i, 0);
    }

    PostMessageW(g_hProgressBar, PBM_SETPOS, 1000, 0);

    auto tEnd = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(tEnd - tStart).count();

    g_converting = false;

    // Send done with packed args via WM_APP_CONV_DONE — use separate approach
    // Store results as globals temporarily for dialog
    static LONGLONG s_orig, s_res;
    static int s_suc, s_fail;
    static double s_elapsed;
    s_orig = totalOrig; s_res = totalResult;
    s_suc = success; s_fail = failed; s_elapsed = elapsed;

    PostMessageW(g_hMainWnd, WM_APP_CONV_DONE,
                 (WPARAM)MAKELONG(success, failed),
                 (LPARAM)&s_elapsed);
}

void StartConversion() {
    if (g_converting) return;
    size_t cnt = 0;
    {
        std::lock_guard<std::mutex> lk(g_itemsMutex);
        cnt = g_items.size();
    }
    if (cnt == 0) {
        MessageBoxW(g_hMainWnd, L"No files to convert.\nDrag & drop files or use context menu.",
                    L"AVIF-Master", MB_ICONINFORMATION);
        return;
    }
    g_converting = true;
    EnableWindow(g_hStartBtn, FALSE);
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Converting...");
    std::thread(ConversionWorker).detach();
}

// ─── Report Dialog ────────────────────────────────────────────────────────────
void ShowReportDialog(HWND hParent, LONGLONG totalOrig, LONGLONG totalResult,
                      int success, int failed, double elapsed) {
    wchar_t msg[512];
    double saved   = totalOrig > 0
                     ? (double)(totalOrig - totalResult) / totalOrig * 100.0
                     : 0.0;
    int mins = (int)(elapsed / 60);
    int secs = (int)(elapsed) % 60;
    int ms   = (int)(elapsed * 1000) % 1000;

    swprintf_s(msg,
        L"Conversion Complete!\r\n"
        L"────────────────────────────\r\n"
        L"  Success : %d    Failed : %d\r\n"
        L"  Elapsed : %02d:%02d.%03d\r\n"
        L"\r\n"
        L"  Original : %s\r\n"
        L"  Result   : %s\r\n"
        L"  Saved    : %.1f%%",
        success, failed,
        mins, secs, ms,
        FormatBytes(totalOrig).c_str(),
        FormatBytes(totalResult).c_str(),
        saved);

    MessageBoxW(hParent, msg, L"AVIF-Master — Report", MB_ICONINFORMATION | MB_OK);
}

// ─── Window Procedure ─────────────────────────────────────────────────────────
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_CREATE:
        CreateMainUI(hwnd);
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_SIZE:
        LayoutWindows(hwnd);
        return 0;

    // ── Virtual ListView data supply ──
    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;

        // Tab switch
        if (nmh->idFrom == IDC_MAIN_TAB && nmh->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_hTab);
            ShowWindow(g_hPerfPanel,     sel == 0 ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hFileMgmtPanel, sel == 1 ? SW_SHOW : SW_HIDE);
            return 0;
        }

        // Virtual list item data — must use W version for Unicode window
        if (nmh->idFrom == IDC_MAIN_LISTVIEW && nmh->code == LVN_GETDISPINFOW) {
            NMLVDISPINFOW* di = (NMLVDISPINFOW*)lParam;
            if (di->item.mask & LVIF_TEXT) {
                std::lock_guard<std::mutex> lk(g_itemsMutex);
                int idx = di->item.iItem;
                if (idx >= 0 && idx < (int)g_items.size()) {
                    const FileItem& it = g_items[idx];
                    const std::wstring* src = nullptr;
                    switch (di->item.iSubItem) {
                        case 0: src = &it.name;         break;
                        case 1: src = &it.type;         break;
                        case 2: src = &it.originalSize; break;
                        case 3: src = &it.statusText;   break;
                        case 4: src = &it.resultSize;   break;
                        case 5: src = &it.ratio;        break;
                    }
                    if (src) {
                        wcsncpy_s(di->item.pszText, di->item.cchTextMax,
                                  src->c_str(), _TRUNCATE);
                    }
                }
            }
            return 0;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == IDC_START_BUTTON && HIWORD(wParam) == BN_CLICKED) {
            StartConversion();
            return 0;
        }
        if (id == IDC_BTN_BROWSE) {
            // Browse for output folder
            wchar_t path[MAX_PATH] = {};
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"Select output folder";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                if (SHGetPathFromIDListW(pidl, path)) {
                    SetWindowTextW(g_hEditOutPath, path);
                    SendMessageW(g_hRadioOutCustom, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessageW(g_hRadioOutSame,   BM_SETCHECK, BST_UNCHECKED, 0);
                    SendMessageW(g_hRadioOutSubdir, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                CoTaskMemFree(pidl);
            }
            return 0;
        }
        if (id == IDC_BTN_SAVE_PROFILE) {
            MessageBoxW(hwnd, L"Profile saved (stub).", L"AVIF-Master", MB_ICONINFORMATION);
            return 0;
        }
        if (id == IDC_BTN_LOAD_PROFILE) {
            MessageBoxW(hwnd, L"Profile loaded (stub).", L"AVIF-Master", MB_ICONINFORMATION);
            return 0;
        }
        break;
    }

    // ── WM_COPYDATA: slave sends files ──
    case WM_COPYDATA: {
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
        if (cds && cds->dwData == MY_COPYDATA_ID && cds->lpData) {
            AddFile((const wchar_t*)cds->lpData);
            std::lock_guard<std::mutex> lk(g_itemsMutex);
            ListView_SetItemCountEx(g_hListView, (int)g_items.size(),
                                    LVSICF_NOINVALIDATEALL);
            InvalidateRect(g_hListView, NULL, FALSE);
        }
        return TRUE;
    }

    // ── Drag & Drop ──
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < n; ++i) {
            UINT sz = DragQueryFileW(hDrop, i, NULL, 0) + 1;
            std::vector<wchar_t> buf(sz);
            DragQueryFileW(hDrop, i, buf.data(), sz);
            AddFile(buf.data());
        }
        DragFinish(hDrop);
        {
            std::lock_guard<std::mutex> lk(g_itemsMutex);
            ListView_SetItemCountEx(g_hListView, (int)g_items.size(),
                                    LVSICF_NOINVALIDATEALL);
        }
        InvalidateRect(g_hListView, NULL, FALSE);
        return 0;
    }

    // ── List update from worker thread ──
    case WM_APP_UPDATE_LIST: {
        std::lock_guard<std::mutex> lk(g_itemsMutex);
        ListView_SetItemCountEx(g_hListView, (int)g_items.size(),
                                LVSICF_NOINVALIDATEALL);
        // Redraw the specific row
        int row = (int)wParam;
        ListView_RedrawItems(g_hListView, row, row);
        UpdateWindow(g_hListView);
        return 0;
    }

    // ── Conversion done ──
    case WM_APP_CONV_DONE: {
        EnableWindow(g_hStartBtn, TRUE);
        SendMessageW(g_hProgressBar, PBM_SETPOS, 1000, 0);
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Done");

        // Collect totals
        LONGLONG orig = 0, res = 0;
        int suc = 0, fail = 0;
        {
            std::lock_guard<std::mutex> lk(g_itemsMutex);
            for (auto& it : g_items) {
                orig += it.originalBytes;
                res  += it.resultBytes;
                if (it.status == ConvStatus::Done)   ++suc;
                if (it.status == ConvStatus::Failed) ++fail;
            }
        }
        double* pel = (double*)lParam;
        double elapsed = pel ? *pel : 0.0;

        ShowReportDialog(hwnd, orig, res, suc, fail, elapsed);

        bool autoClose = (SendMessageW(g_hCheckAutoClose, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (autoClose) DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        if (g_converting) {
            if (MessageBoxW(hwnd,
                    L"Conversion in progress. Exit anyway?",
                    L"AVIF-Master", MB_YESNO | MB_ICONWARNING) != IDYES)
                return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
