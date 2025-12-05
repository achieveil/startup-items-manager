#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <cstdio>
#include <unordered_set>
#include <algorithm>
#include <shobjidl.h>
#include <gdiplus.h>
#include "../res/resource.h"

using namespace Gdiplus;

// 链接库
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 控件 ID
enum : int
{
    IDC_LISTVIEW = 1001,
    IDC_BTN_ENABLE,
    IDC_BTN_DISABLE,
    IDC_BTN_DELETE,
    IDC_BTN_ADD,
    IDC_BTN_OPEN,
    IDC_BTN_REFRESH,
    IDC_STATUSBAR
};

// 颜色定义
const COLORREF CLR_BG = RGB(243, 243, 243);         // 窗口背景浅灰
const COLORREF CLR_BTN_NORMAL = RGB(255, 255, 255); // 普通按钮白底
const COLORREF CLR_BTN_PRIMARY = RGB(0, 103, 192);  // 添加按钮
const COLORREF CLR_TEXT_NORMAL = RGB(0, 0, 0);
const COLORREF CLR_TEXT_WHITE = RGB(255, 255, 255);
const COLORREF CLR_BORDER = RGB(204, 204, 204);     // 按钮边框
const COLORREF CLR_TOGGLE_ON = RGB(16, 124, 16);    // 开关绿色
const COLORREF CLR_TOGGLE_OFF = RGB(200, 200, 200); // 开关灰色

// 注册表路径
const wchar_t *kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t *kStartupApproved = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
const wchar_t *kDisabledRunKey = L"Software\\StartClean\\DisabledRun";

// 数据结构
struct StartupItem
{
    enum class LocationType
    {
        Registry,
        Folder
    };
    LocationType type{};
    HKEY rootKey{nullptr};
    std::wstring keyPath;
    std::wstring disabledKeyPath;
    std::wstring folderPath;
    std::wstring disabledFolderPath;
    std::wstring name;
    std::wstring command;
    std::wstring resolvedPath;
    std::wstring publisher;
    DWORD regSam{0};
    bool enabled{true};
    int iconIndex{-1};
    bool requiresAdmin{false};
};

// 全局变量
HINSTANCE g_hInst = nullptr;
HWND g_hMainWnd = nullptr;
HWND g_hList = nullptr;
HWND g_hStatus = nullptr;
HIMAGELIST g_hImageList = nullptr;  // 用于列表图标
HIMAGELIST g_hSpacerList = nullptr; // 用于强制撑开行高
std::vector<StartupItem> g_items;
bool g_loading = false;
HFONT g_hUIFont = nullptr;
HFONT g_hBoldFont = nullptr;
ULONG_PTR g_gdiplusToken = 0;
bool g_firstLayoutDone = false;
UINT g_dpi = 96;

// --- 辅助函数 (保持业务逻辑不变) ---
void EnableDpiAwareness()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

HFONT CreateUIFont(int sizePt, bool bold)
{
    long lfHeight = -MulDiv(sizePt, g_dpi, 72);
    return CreateFontW(lfHeight, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// 简单的高 DPI 缩放辅助
int Scale(int x)
{
    return MulDiv(x, g_dpi, 96);
}

// 业务逻辑 Helper (精简版，功能与原版一致)
std::wstring Trim(const std::wstring &s)
{
    size_t first = s.find_first_not_of(L" \t");
    if (std::wstring::npos == first)
        return s;
    size_t last = s.find_last_not_of(L" \t");
    return s.substr(first, (last - first + 1));
}
std::wstring ExtractPath(const std::wstring &cmd)
{
    std::wstring c = Trim(cmd);
    if (c.empty())
        return L"";
    if (c.front() == L'"')
    {
        size_t p = c.find(L'"', 1);
        if (p != std::wstring::npos)
            return c.substr(1, p - 1);
    }
    size_t p = c.find(L' ');
    return (p == std::wstring::npos) ? c : c.substr(0, p);
}
std::wstring ExpandEnv(const std::wstring &s)
{
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), NULL, 0);
    if (n == 0)
        return s;
    std::wstring out(n, 0);
    ExpandEnvironmentStringsW(s.c_str(), &out[0], n);
    if (out.back() == 0)
        out.pop_back();
    return out;
}
std::wstring GetFileName(const std::wstring &p)
{
    wchar_t f[_MAX_FNAME], e[_MAX_EXT];
    _wsplitpath_s(p.c_str(), 0, 0, 0, 0, f, _MAX_FNAME, e, _MAX_EXT);
    return std::wstring(f) + e;
}
std::wstring GetPublisher(const std::wstring &path)
{
    if (path.empty() || !PathFileExistsW(path.c_str()))
        return L"";
    DWORD h = 0, s = GetFileVersionInfoSizeW(path.c_str(), &h);
    if (s == 0)
        return L"";
    std::vector<BYTE> d(s);
    if (!GetFileVersionInfoW(path.c_str(), 0, s, d.data()))
        return L"";
    struct LANG
    {
        WORD wL;
        WORD wC;
    } *pTr;
    UINT l = 0;
    if (!VerQueryValueW(d.data(), L"\\VarFileInfo\\Translation", (LPVOID *)&pTr, &l) || l == 0)
        return L"";
    wchar_t blk[64];
    swprintf_s(blk, L"\\StringFileInfo\\%04x%04x\\CompanyName", pTr[0].wL, pTr[0].wC);
    LPVOID buf = 0;
    UINT len = 0;
    if (VerQueryValueW(d.data(), blk, &buf, &len) && len > 0)
        return (wchar_t *)buf;
    return L"";
}
int GetIconIdx(const std::wstring &path)
{
    SHFILEINFOW sfi{};
    if (!SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON))
        return -1;
    int idx = ImageList_AddIcon(g_hImageList, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return idx;
}

std::wstring ResolveLnkTarget(const std::wstring &lnk)
{
    std::wstring result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hr);
    IShellLinkW *psl = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl))))
    {
        IPersistFile *ppf = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_PPV_ARGS(&ppf))))
        {
            if (SUCCEEDED(ppf->Load(lnk.c_str(), STGM_READ)))
            {
                wchar_t target[MAX_PATH];
                WIN32_FIND_DATAW wfd{};
                if (SUCCEEDED(psl->GetPath(target, MAX_PATH, &wfd, SLGP_UNCPRIORITY)))
                {
                    result = target;
                }
            }
            ppf->Release();
        }
        psl->Release();
    }
    if (needUninit)
        CoUninitialize();
    return result;
}
bool EnsureDir(const std::wstring &p)
{
    if (PathFileExistsW(p.c_str()))
        return true;
    DWORD res = SHCreateDirectoryExW(NULL, p.c_str(), NULL);
    if (res == ERROR_SUCCESS || res == ERROR_ALREADY_EXISTS)
        return true;
    SetLastError(res);
    return false;
}

bool ExistsItem(const StartupItem &cand)
{
    for (const auto &it : g_items)
    {
        if (it.type == cand.type && _wcsicmp(it.name.c_str(), cand.name.c_str()) == 0 &&
            _wcsicmp(it.resolvedPath.c_str(), cand.resolvedPath.c_str()) == 0)
        {
            return true;
        }
    }
    return false;
}

bool ShouldSkipFile(const WIN32_FIND_DATAW &fd)
{
    if (_wcsicmp(fd.cFileName, L"desktop.ini") == 0)
        return true;
    if (fd.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))
        return true;
    return false;
}

bool AlreadyHasItem(HKEY root, const std::wstring &key, const std::wstring &name, DWORD /*sam*/)
{
    for (const auto &it : g_items)
    {
        if (it.type == StartupItem::LocationType::Registry &&
            it.rootKey == root && it.keyPath == key && it.name == name)
            return true;
    }
    return false;
}

// --- 注册表与文件操作 (精简核心逻辑) ---
bool MovReg(HKEY root, const std::wstring &src, const std::wstring &dst, const std::wstring &name, DWORD sam)
{
    HKEY hS = nullptr, hD = nullptr;
    LONG err = RegOpenKeyExW(root, src.c_str(), 0, KEY_READ | KEY_WRITE | sam, &hS);
    if (err != ERROR_SUCCESS)
    {
        SetLastError((DWORD)err);
        return false;
    }
    err = RegCreateKeyExW(root, dst.c_str(), 0, NULL, 0, KEY_WRITE | sam, NULL, &hD, NULL);
    if (err != ERROR_SUCCESS)
    {
        RegCloseKey(hS);
        SetLastError((DWORD)err);
        return false;
    }
    DWORD t = 0, s = 0;
    err = RegQueryValueExW(hS, name.c_str(), NULL, &t, NULL, &s);
    if (err == ERROR_SUCCESS)
    {
        std::vector<BYTE> b(s);
        err = RegQueryValueExW(hS, name.c_str(), NULL, &t, b.data(), &s);
        if (err == ERROR_SUCCESS)
        {
            err = RegSetValueExW(hD, name.c_str(), 0, t, b.data(), s);
            if (err == ERROR_SUCCESS)
                RegDeleteValueW(hS, name.c_str());
        }
    }
    RegCloseKey(hS);
    RegCloseKey(hD);
    if (err != ERROR_SUCCESS)
        SetLastError((DWORD)err);
    return err == ERROR_SUCCESS;
}

bool ToggleItem(const StartupItem &item, bool enable)
{
    if (item.enabled == enable)
        return true;
    SetLastError(ERROR_SUCCESS);
    bool ok = false;
    if (item.type == StartupItem::LocationType::Registry)
    {
        ok = enable ? MovReg(item.rootKey, item.disabledKeyPath, kRunKey, item.name, item.regSam)
                    : MovReg(item.rootKey, item.keyPath, item.disabledKeyPath, item.name, item.regSam);
    }
    else
    {
        if (!EnsureDir(item.disabledFolderPath))
            return false;
        std::wstring src = item.command;
        std::wstring dst = item.disabledFolderPath + L"\\" + item.name;
        ok = MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    }
    return ok;
}

bool DelItem(const StartupItem &item)
{
    DWORD err = ERROR_SUCCESS;
    if (item.type == StartupItem::LocationType::Registry)
    {
        auto deleteValue = [&](const std::wstring &keyPath) -> LONG
        {
            HKEY h = nullptr;
            LONG ret = RegOpenKeyExW(item.rootKey, keyPath.c_str(), 0, KEY_WRITE | item.regSam, &h);
            if (ret == ERROR_SUCCESS)
            {
                LONG rd = RegDeleteValueW(h, item.name.c_str());
                RegCloseKey(h);
                if (rd == ERROR_FILE_NOT_FOUND)
                    rd = ERROR_SUCCESS; // 不存在视为已删除
                return rd;
            }
            if (ret == ERROR_FILE_NOT_FOUND || ret == ERROR_PATH_NOT_FOUND)
                return ERROR_SUCCESS; // 没有该键视为已删除
            return ret;
        };
        LONG r1 = deleteValue(kRunKey);
        LONG r2 = deleteValue(kDisabledRunKey);
        if (r1 != ERROR_SUCCESS)
            err = (DWORD)r1;
        else if (r2 != ERROR_SUCCESS)
            err = (DWORD)r2;
    }
    else
    {
        auto deleteFileMaybeMissing = [](const std::wstring &path) -> DWORD
        {
            if (path.empty())
                return ERROR_SUCCESS;
            if (DeleteFileW(path.c_str()))
                return ERROR_SUCCESS;
            DWORD e = GetLastError();
            if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)
                return ERROR_SUCCESS;
            return e;
        };
        err = deleteFileMaybeMissing(item.command);
        if (err == ERROR_SUCCESS)
        {
            std::wstring disabled = item.disabledFolderPath + L"\\" + GetFileName(item.command);
            err = deleteFileMaybeMissing(disabled);
        }
    }
    if (err != ERROR_SUCCESS)
    {
        SetLastError(err);
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    return true;
}

bool RequiresAdmin(const StartupItem &item)
{
    return item.requiresAdmin;
}

void HandleOpResult(HWND hwnd, const StartupItem &item, bool ok, const wchar_t *action)
{
    if (ok)
        return;
    DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED && RequiresAdmin(item))
    {
        wchar_t msg[256];
        swprintf_s(msg, L"%s需要管理员权限，是否以管理员身份重新启动应用后重试？", action);
        if (MessageBoxW(hwnd, msg, L"需要管理员权限", MB_ICONQUESTION | MB_YESNO) == IDYES)
        {
            wchar_t exe[MAX_PATH];
            if (GetModuleFileNameW(NULL, exe, MAX_PATH))
            {
                HINSTANCE h = ShellExecuteW(NULL, L"runas", exe, NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)h <= 32)
                    MessageBoxW(hwnd, L"无法启动管理员权限，请手动以管理员运行。", L"提示", MB_ICONWARNING);
            }
        }
    }
    else
    {
        wchar_t msg[256];
        swprintf_s(msg, L"%s失败 (错误码 %lu)。", action, err);
        MessageBoxW(hwnd, msg, L"操作失败", MB_ICONERROR);
    }
}

// --- 数据加载 ---
void EnumReg(HKEY root, const std::wstring &key, bool en, DWORD sam)
{
    HKEY h;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_READ | sam, &h) != 0)
        return;
    DWORD i = 0, ns = 256, ds = 2048, t;
    wchar_t n[256];
    BYTE d[2048];
    while (RegEnumValueW(h, i++, n, &ns, NULL, &t, d, &ds) == 0)
    {
        ns = 256;
        ds = 2048;
        if (t != REG_SZ && t != REG_EXPAND_SZ)
            continue;
        std::wstring cmd((wchar_t *)d, ds / 2);
        if (!cmd.empty() && cmd.back() == 0)
            cmd.pop_back();
        std::wstring path = ExtractPath(ExpandEnv(cmd));
        if (AlreadyHasItem(root, key, n, sam))
            continue;
        StartupItem it;
        it.type = StartupItem::LocationType::Registry;
        it.rootKey = root;
        it.keyPath = key;
        it.disabledKeyPath = kDisabledRunKey;
        it.name = n;
        it.command = cmd;
        it.resolvedPath = path;
        it.publisher = GetPublisher(path);
        it.enabled = en;
        it.iconIndex = GetIconIdx(path);
        it.regSam = sam;
        it.requiresAdmin = (root == HKEY_LOCAL_MACHINE);
        if (!ExistsItem(it))
            g_items.push_back(it);
    }
    RegCloseKey(h);
}

void EnumFolder(const std::wstring &folder, const std::wstring &disabled, bool enabled, bool requiresAdmin)
{
    if (folder.empty() || !PathFileExistsW(folder.c_str()))
        return;
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW((folder + L"\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if (ShouldSkipFile(fd))
            continue;
        StartupItem it;
        it.type = StartupItem::LocationType::Folder;
        it.folderPath = folder;
        it.disabledFolderPath = disabled;
        it.name = fd.cFileName;
        it.command = folder + L"\\" + fd.cFileName;
        it.resolvedPath = it.command;
        std::wstring target;
        if (StrStrIW(it.name.c_str(), L".lnk") != nullptr)
        {
            target = ResolveLnkTarget(it.command);
            if (!target.empty())
                it.resolvedPath = target;
        }
        it.publisher = GetPublisher(it.resolvedPath);
        it.enabled = enabled;
        it.iconIndex = GetIconIdx(it.resolvedPath);
        it.requiresAdmin = requiresAdmin;
        if (!ExistsItem(it))
            g_items.push_back(std::move(it));
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void LoadItems()
{
    g_loading = true;
    g_items.clear();
    if (g_hImageList)
        ImageList_Destroy(g_hImageList);
    g_hImageList = ImageList_Create(Scale(32), Scale(32), ILC_COLOR32 | ILC_MASK, 16, 16);
    ListView_SetImageList(g_hList, g_hImageList, LVSIL_SMALL);

    const DWORD views[] = {0, KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    for (DWORD sam : views)
    {
        EnumReg(HKEY_CURRENT_USER, kRunKey, true, sam);
        EnumReg(HKEY_LOCAL_MACHINE, kRunKey, true, sam);
        EnumReg(HKEY_CURRENT_USER, kDisabledRunKey, false, sam);
        EnumReg(HKEY_LOCAL_MACHINE, kDisabledRunKey, false, sam);
    }
    // Startup folders
    PWSTR p1 = nullptr, p2 = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, 0, &p1)))
    {
        std::wstring folder = p1;
        std::wstring disabled = folder + L"\\StartCleanDisabled";
        EnumFolder(folder, disabled, true, false);
        EnumFolder(disabled, folder, false, false);
        CoTaskMemFree(p1);
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, 0, &p2)))
    {
        std::wstring folder = p2;
        std::wstring disabled = folder + L"\\StartCleanDisabled";
        EnumFolder(folder, disabled, true, true);
        EnumFolder(disabled, folder, false, true);
        CoTaskMemFree(p2);
    }

    // 稳定排序（按名称），避免刷新后顺序跳动
    std::stable_sort(g_items.begin(), g_items.end(), [](const StartupItem &a, const StartupItem &b)
                     { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });

    ListView_DeleteAllItems(g_hList);
    for (size_t i = 0; i < g_items.size(); ++i)
    {
        LVITEMW lv{};
        lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lv.iItem = (int)i;
        lv.pszText = LPSTR_TEXTCALLBACK;
        lv.iImage = I_IMAGECALLBACK;
        lv.lParam = (LPARAM)i;
        ListView_InsertItem(g_hList, &lv);
    }
    g_loading = false;

    wchar_t buf[64];
    swprintf_s(buf, L" 共加载 %zu 个启动项", g_items.size());
    SendMessageW(g_hStatus, SB_SETTEXT, 0, (LPARAM)buf);
}

// --- 操作辅助 ---
void HandleAdd(HWND hwnd)
{
    wchar_t fileBuffer[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"可执行文件 (*.exe;*.lnk)\0*.exe;*.lnk\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn))
        return;
    std::wstring path = fileBuffer;
    std::wstring name = GetFileName(path);
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == 0)
    {
        RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, (const BYTE *)path.c_str(), (DWORD)((path.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
    LoadItems();
}

void HandleOpen()
{
    int idx = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)g_items.size())
        return;
    std::wstring target = g_items[idx].resolvedPath.empty() ? g_items[idx].command : g_items[idx].resolvedPath;
    if (target.empty() && StrStrIW(g_items[idx].command.c_str(), L".lnk"))
    {
        target = ResolveLnkTarget(g_items[idx].command);
    }
    if (!target.empty() && PathFileExistsW(target.c_str()))
    {
        wchar_t folder[MAX_PATH];
        wcsncpy_s(folder, target.c_str(), _TRUNCATE);
        PathRemoveFileSpecW(folder);
        if (PathFileExistsW(folder))
        {
            ShellExecuteW(NULL, L"open", folder, NULL, NULL, SW_SHOWNORMAL);
        }
    }
}

// 统一布局逻辑
void LayoutUI(HWND hwnd, int w, int h)
{
    int pad = Scale(12);
    int btnGap = Scale(8);
    int listGap = Scale(8);
    int btnW = Scale(90);
    int btnH = Scale(32);

    int x = pad;
    int y = pad;

    const int ids[] = {IDC_BTN_ENABLE, IDC_BTN_DISABLE, IDC_BTN_DELETE};
    for (int id : ids)
    {
        MoveWindow(GetDlgItem(hwnd, id), x, y, btnW, btnH, TRUE);
        x += btnW + btnGap;
    }

    MoveWindow(GetDlgItem(hwnd, IDC_BTN_ADD), x, y, btnW, btnH, TRUE);
    x += btnW + btnGap;

    MoveWindow(GetDlgItem(hwnd, IDC_BTN_OPEN), x, y, Scale(110), btnH, TRUE);

    MoveWindow(GetDlgItem(hwnd, IDC_BTN_REFRESH), w - pad - btnW, y, btnW, btnH, TRUE);

    RECT rcStatus;
    GetWindowRect(g_hStatus, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;

    int listTop = y + btnH + listGap;
    int listH = h - listTop - statusH;

    MoveWindow(g_hList, pad, listTop, w - pad * 2, listH, TRUE);

    int wStatus = Scale(96);
    int wPub = Scale(240);
    int clientW = w - pad * 2;
    int wName = clientW - wStatus - wPub;
    if (wName > Scale(80))
    {
        ListView_SetColumnWidth(g_hList, 0, wName);
        ListView_SetColumnWidth(g_hList, 1, wPub);
        ListView_SetColumnWidth(g_hList, 2, wStatus);
    }
    SendMessage(g_hStatus, WM_SIZE, 0, 0);
}

// --- 自绘按钮逻辑 ---
void DrawRoundedButton(LPDRAWITEMSTRUCT dis, const wchar_t *text, bool isPrimary)
{
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pressed = (dis->itemState & ODS_SELECTED);
    bool hot = (dis->itemState & ODS_HOTLIGHT) || (dis->itemState & ODS_FOCUS); // 简单处理焦点为高亮

    // 双缓冲
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBm = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBm);

    // 填充父窗口背景
    RECT rcAll = {0, 0, rc.right - rc.left, rc.bottom - rc.top};
    HBRUSH bgBrush = CreateSolidBrush(CLR_BG);
    FillRect(memDC, &rcAll, bgBrush);
    DeleteObject(bgBrush);

    // 设置抗锯齿
    SetBkMode(memDC, TRANSPARENT);

    // 按钮颜色
    COLORREF fill = isPrimary ? CLR_BTN_PRIMARY : CLR_BTN_NORMAL;
    COLORREF border = isPrimary ? CLR_BTN_PRIMARY : CLR_BORDER;
    COLORREF textClr = isPrimary ? CLR_TEXT_WHITE : CLR_TEXT_NORMAL;

    if (pressed)
    {
        // 按下变暗一点
        fill = isPrimary ? RGB(0, 90, 170) : RGB(240, 240, 240);
    }

    // 绘制圆角矩形
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hBrush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(memDC, hPen);
    HGDIOBJ oldBrush = SelectObject(memDC, hBrush);

    int corner = Scale(6); // 圆角大小
    RoundRect(memDC, 0, 0, rcAll.right, rcAll.bottom, corner, corner);

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);

    // 绘制文字
    HFONT fontToUse = g_hUIFont;
    if (isPrimary)
        fontToUse = g_hBoldFont; // 主按钮加粗

    SelectObject(memDC, fontToUse);
    SetTextColor(memDC, textClr);
    DrawTextW(memDC, text, -1, &rcAll, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // BitBlt 回去
    BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, hOld);
    DeleteObject(hBm);
    DeleteDC(memDC);
}

// --- 列表自定义绘制 ---
LRESULT HandleCustomDraw(LPNMLVCUSTOMDRAW lplvcd)
{
    switch (lplvcd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW; // 通知子项绘制
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
    {
        int row = (int)lplvcd->nmcd.dwItemSpec;
        int col = lplvcd->iSubItem;
        if (row >= (int)g_items.size())
            return CDRF_DODEFAULT;

        HDC hdc = lplvcd->nmcd.hdc;
        RECT rc = lplvcd->nmcd.rc;
        const auto &item = g_items[row];

        // 1. 背景：选中浅蓝，否则纯白；
        if (col == 0)
        {
            RECT rcRow;
            ListView_GetItemRect(g_hList, row, &rcRow, LVIR_BOUNDS);
            bool selected = (ListView_GetItemState(g_hList, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            COLORREF bg = selected ? RGB(235, 245, 255) : RGB(255, 255, 255);
            HBRUSH hBr = CreateSolidBrush(bg);
            FillRect(hdc, &rcRow, hBr);
            DeleteObject(hBr);

            int w0 = ListView_GetColumnWidth(g_hList, 0);
            int w1 = ListView_GetColumnWidth(g_hList, 1);
            int boundary1 = rcRow.left + w0 - 1;
            int boundary2 = rcRow.left + w0 + w1 - 1;
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(225, 225, 225));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            MoveToEx(hdc, boundary1, rcRow.top, nullptr);
            LineTo(hdc, boundary1, rcRow.bottom);
            MoveToEx(hdc, boundary2, rcRow.top, nullptr);
            LineTo(hdc, boundary2, rcRow.bottom);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }

        // 2. 绘制内容
        if (col == 0)
        { // 名称 + 图标
            int pad = Scale(10);
            int iconSize = Scale(26);
            int iconX = rc.left + pad;
            int iconY = rc.top + (rc.bottom - rc.top - iconSize) / 2;
            if (g_hImageList && item.iconIndex >= 0)
            {
                ImageList_Draw(g_hImageList, item.iconIndex, hdc, iconX, iconY, ILD_TRANSPARENT);
            }
            RECT rcText = rc;
            rcText.left = iconX + iconSize + pad;
            rcText.right -= pad;
            rcText.top += pad / 4;
            rcText.bottom -= pad / 4;
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hUIFont);
            SetTextColor(hdc, CLR_TEXT_NORMAL);
            DrawTextW(hdc, item.name.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        else if (col == 1)
        { // 发布者
            RECT rcText = rc;
            int pad = Scale(10);
            rcText.left += pad;
            rcText.right -= pad;
            rcText.top += pad / 3;
            rcText.bottom -= pad / 3;
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hUIFont);
            SetTextColor(hdc, RGB(100, 100, 100)); // 灰色文字
            DrawTextW(hdc, item.publisher.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        else if (col == 2)
        { // 状态 (开关)
            int pad = Scale(8);
            int width = Scale(42);
            int height = Scale(22);
            int x = rc.left + (rc.right - rc.left - width) / 2;
            int y = rc.top + (rc.bottom - rc.top - height) / 2;

            Graphics g(hdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);

            Color trackColor(item.enabled ? 255 : 255,
                             GetRValue(item.enabled ? CLR_TOGGLE_ON : CLR_TOGGLE_OFF),
                             GetGValue(item.enabled ? CLR_TOGGLE_ON : CLR_TOGGLE_OFF),
                             GetBValue(item.enabled ? CLR_TOGGLE_ON : CLR_TOGGLE_OFF));
            Color thumbColor(255, 255, 255, 255);

            GraphicsPath path;
            int radius = height;
            path.AddArc(x, y, radius, height, 90, 180);
            path.AddArc(x + width - radius, y, radius, height, -90, 180);
            path.CloseFigure();
            SolidBrush trackBrush(trackColor);
            g.FillPath(&trackBrush, &path);

            int circleSize = height - Scale(4);
            int cx = item.enabled ? (x + width - circleSize - Scale(2)) : (x + Scale(2));
            int cy = y + Scale(2);
            SolidBrush thumbBrush(thumbColor);
            g.FillEllipse(&thumbBrush, Rect(cx, cy, circleSize, circleSize));
        }
        return CDRF_SKIPDEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

// --- 主窗口消息处理 ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi)
            g_dpi = dpi;
        InitCommonControls();
        g_hUIFont = CreateUIFont(10, false);
        g_hBoldFont = CreateUIFont(10, true);

        // 创建按钮 (BS_OWNERDRAW)
        auto CreateBtn = [&](int id, const wchar_t *txt, int x, int y, int w)
        {
            CreateWindowW(L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                          x, y, w, Scale(32), hwnd, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };

        // 顶部工具栏
        CreateBtn(IDC_BTN_ENABLE, L"启用", 0, 0, 0);
        CreateBtn(IDC_BTN_DISABLE, L"禁用", 0, 0, 0);
        CreateBtn(IDC_BTN_DELETE, L"删除", 0, 0, 0);
        CreateBtn(IDC_BTN_ADD, L"添加", 0, 0, 0);
        CreateBtn(IDC_BTN_OPEN, L"打开所在位置", 0, 0, 0);
        CreateBtn(IDC_BTN_REFRESH, L"刷新", 0, 0, 0);

        // 列表控件
        g_hList = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
                                  WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_OWNERDATA,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_LISTVIEW, g_hInst, nullptr);

        // 样式设置
        ListView_SetExtendedListViewStyleEx(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        // 去除边框，让其融入背景
        SetWindowTheme(g_hList, L"Explorer", nullptr);

        // 列头
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_LEFT;
        col.pszText = (LPWSTR)L"  名称";
        col.cx = Scale(380);
        ListView_InsertColumn(g_hList, 0, &col);
        col.pszText = (LPWSTR)L"  发布者";
        col.cx = Scale(250);
        col.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(g_hList, 1, &col);
        col.pszText = (LPWSTR)L"状态";
        col.cx = Scale(96);
        col.fmt = LVCFMT_CENTER;
        ListView_InsertColumn(g_hList, 2, &col);

        // 强制表头居中对齐
        HWND hHeader = ListView_GetHeader(g_hList);
        if (hHeader)
        {
            HDITEMW hd{};
            hd.mask = HDI_FORMAT;
            for (int i = 0; i < 3; ++i)
            {
                if (SendMessageW(hHeader, HDM_GETITEMW, i, (LPARAM)&hd))
                {
                    hd.fmt &= ~(HDF_LEFT | HDF_RIGHT | HDF_CENTER);
                    if (i == 2)
                        hd.fmt |= HDF_CENTER; // 状态列居中
                    else
                        hd.fmt |= HDF_LEFT; // 名称/发布者左对齐
                    SendMessageW(hHeader, HDM_SETITEMW, i, (LPARAM)&hd);
                }
            }
        }

        // 使用一个透明的 ImageList 撑大行高 (Hack)
        g_hSpacerList = ImageList_Create(1, Scale(40), ILC_COLOR, 1, 1); // 40px 高度
        ListView_SetImageList(g_hList, g_hSpacerList, LVSIL_STATE);      // 设置为状态图标以撑开高度，但不实际显示

        // 状态栏
        g_hStatus = CreateStatusWindowW(WS_CHILD | WS_VISIBLE, L"", hwnd, IDC_STATUSBAR);

        LoadItems();

        // 初始化时手动触发布局，避免首次显示列宽/控件错位
        RECT rc{};
        GetClientRect(hwnd, &rc);
        SendMessageW(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
        return 0;
    }

    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        LayoutUI(hwnd, w, h);
        return 0;
    }

    case WM_DRAWITEM: // 绘制按钮
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_BUTTON)
        {
            bool isPrimary = (dis->CtlID == IDC_BTN_ADD); // "添加" 按钮为蓝色
            wchar_t txt[64];
            GetWindowTextW(dis->hwndItem, txt, 64);
            DrawRoundedButton(dis, txt, isPrimary);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->hwndFrom == g_hList)
        {
            if (hdr->code == NM_CUSTOMDRAW)
            {
                return HandleCustomDraw((LPNMLVCUSTOMDRAW)lParam);
            }
            else if (hdr->code == NM_CLICK || hdr->code == NM_DBLCLK)
            {
                LPNMITEMACTIVATE ia = (LPNMITEMACTIVATE)lParam;
                if (ia->iItem >= 0 && ia->iItem < (int)g_items.size())
                {
                    // 如果点击的是第2列(状态列)，则切换开关
                    if (ia->iSubItem == 2 || hdr->code == NM_DBLCLK)
                    {
                        const StartupItem item = g_items[ia->iItem];
                        bool ok = ToggleItem(item, !item.enabled);
                        HandleOpResult(hwnd, item, ok, L"切换状态");
                        LoadItems(); // 重新加载以刷新 UI
                    }
                }
            }
        }
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_ENABLE:
        {
            int idx = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
            if (idx >= 0)
            {
                const StartupItem item = g_items[idx];
                bool ok = ToggleItem(item, true);
                HandleOpResult(hwnd, item, ok, L"启用");
                LoadItems();
            }
            break;
        }
        case IDC_BTN_DISABLE:
        {
            int idx = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
            if (idx >= 0)
            {
                const StartupItem item = g_items[idx];
                bool ok = ToggleItem(item, false);
                HandleOpResult(hwnd, item, ok, L"禁用");
                LoadItems();
            }
            break;
        }
        case IDC_BTN_DELETE:
        {
            int idx = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
            if (idx >= 0 && MessageBoxW(hwnd, L"确认删除?", L"提示", MB_YESNO) == IDYES)
            {
                const StartupItem item = g_items[idx];
                bool ok = DelItem(item);
                HandleOpResult(hwnd, item, ok, L"删除");
                LoadItems();
            }
            break;
        }
        case IDC_BTN_ADD:
            HandleAdd(hwnd);
            break;
        case IDC_BTN_OPEN:
            HandleOpen();
            break;
        case IDC_BTN_REFRESH:
            LoadItems();
            break;
        }
        return 0;

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO p = (LPMINMAXINFO)lParam;
        p->ptMinTrackSize.x = Scale(760);
        p->ptMinTrackSize.y = Scale(480);
        return 0;
    }
    case WM_SHOWWINDOW:
    {
        if (wParam && !g_firstLayoutDone)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            LayoutUI(hwnd, rc.right - rc.left, rc.bottom - rc.top);
            g_firstLayoutDone = true;
        }
        break;
    }

    case WM_DESTROY:
        DeleteObject(g_hUIFont);
        DeleteObject(g_hBoldFont);
        if (g_hImageList)
            ImageList_Destroy(g_hImageList);
        if (g_hSpacerList)
            ImageList_Destroy(g_hSpacerList);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    EnableDpiAwareness();
    UINT dpi = GetDpiForSystem();
    g_dpi = dpi ? dpi : 96;
    g_hInst = hInstance;
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok)
        g_gdiplusToken = 0;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ModernStartClean";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = CreateSolidBrush(CLR_BG); // 窗口背景色
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(0, wc.lpszClassName, L"StartClean 启动项管理", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, Scale(850), Scale(550), NULL, NULL, hInstance, NULL);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    // 使用 PostMessage 触发一次 WM_SIZE，确保初始布局
    RECT rcInit;
    GetClientRect(g_hMainWnd, &rcInit);
    PostMessageW(g_hMainWnd, WM_SIZE, 0, MAKELPARAM(rcInit.right - rcInit.left, rcInit.bottom - rcInit.top));

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (g_gdiplusToken)
        GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
