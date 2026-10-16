// Native Windows output fixture. Build with MSVC or MinGW, then run under
// Windhawk with installer-fixture.exe explicitly added to Installer programs.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

static HBRUSH neutralBrush, colorBrush, customBrush;
static HBITMAP artwork;
static HFONT fixtureFont;
static std::wstring outputPrefix;
static bool expectDark;
static std::wstring engine = L"classic";
static bool interactive = false;
static WNDPROC nativeStaticProc, nativeEditProc, nativeButtonProc;
static int result = 3;
static unsigned licenseBluePixels;
static bool licenseLightBackground;

static HWND AddControl(HWND dialog, const wchar_t* cls, const wchar_t* text,
    DWORD style, int x, int y, int width, int height, int id)
{
    if (engine == L"inno") {
        if (!wcscmp(cls, L"Static") && !(style & SS_BITMAP)) cls = L"TNewStaticText";
        else if (!wcscmp(cls, L"Edit")) cls = L"TNewEdit";
        else if (!wcscmp(cls, L"Button")) cls = L"TNewButton";
    }
    HWND control = CreateWindowExW(0, cls, text,
        WS_CHILD | WS_VISIBLE | style, x, y, width, height, dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(fixtureFont), TRUE);
    return control;
}

static LRESULT CALLBACK CustomPainter(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);
        RECT rect;
        GetClientRect(window, &rect);
        FillRect(dc, &rect, customBrush);
        EndPaint(window, &ps);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}

static bool SaveCapture(HWND dialog, const std::wstring& path, COLORREF* samples)
{
    RECT rc;
    GetClientRect(dialog, &rc);
    HDC screen = GetDC(dialog);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rc.right;
    info.bmiHeader.biHeight = -rc.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HGDIOBJ previous = bitmap ? SelectObject(memory, bitmap) : nullptr;
    bool okay = bitmap && pixels && BitBlt(memory, 0, 0, rc.right, rc.bottom, screen, 0, 0, SRCCOPY);
    if (okay) {
        // Assert actual authored license glyphs, not merely empty panel pixels.
        licenseBluePixels = 0;
        const COLORREF licenseBg = GetPixel(memory, 400, 180);
        licenseLightBackground = GetRValue(licenseBg) >= 240 &&
            GetGValue(licenseBg) >= 240 && GetBValue(licenseBg) >= 240;
        for (int y = 148; y < 174; ++y)
            for (int x = 23; x < 405; ++x) {
                const COLORREF c = GetPixel(memory, x, y);
                if (GetBValue(c) > GetRValue(c) + 40 &&
                    GetBValue(c) > GetGValue(c) + 40)
                    ++licenseBluePixels;
            }
        // Empty background points, away from glyph antialiasing and borders.
        const POINT points[] = {{15, 225}, {305, 25}, {305, 65}, {30, 109}, {150, 109}};
        for (size_t i = 0; i < std::size(points); ++i)
            samples[i] = GetPixel(memory, points[i].x, points[i].y);
        FILE* file = nullptr;
        _wfopen_s(&file, path.c_str(), L"wb");
        if (!file) okay = false;
        else {
            const DWORD bytes = rc.right * rc.bottom * 4;
            BITMAPFILEHEADER header = {};
            header.bfType = 0x4D42;
            header.bfSize = sizeof(header) + sizeof(info.bmiHeader) + bytes;
            header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
            okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
                fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file) == 1 &&
                fwrite(pixels, bytes, 1, file) == 1;
            if (fclose(file)) okay = false;
        }
    }
    if (previous) SelectObject(memory, previous);
    if (bitmap) DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(dialog, screen);
    return okay;
}

static LRESULT FixtureColor(HWND control, HDC dc)
{
    const bool colored = GetDlgCtrlID(control) == 1002;
    SetBkColor(dc, colored ? RGB(229,191,120) : RGB(245,245,245));
    SetTextColor(dc, RGB(0,0,0));
    return reinterpret_cast<LRESULT>(colored ? colorBrush : neutralBrush);
}

// A synthetic VCL-style reflected color route, not a copy of the Delphi VCL.
static LRESULT CALLBACK InnoControlProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg >= 0xBC00 + WM_CTLCOLORMSGBOX && msg <= 0xBC00 + WM_CTLCOLORSTATIC)
        return FixtureColor(hwnd, reinterpret_cast<HDC>(wp));
    wchar_t cls[32]; GetClassNameW(hwnd, cls, 32);
    WNDPROC original = !wcscmp(cls,L"TNewStaticText") ? nativeStaticProc :
        !wcscmp(cls,L"TNewEdit") ? nativeEditProc : nativeButtonProc;
    return CallWindowProcW(original, hwnd, msg, wp, lp);
}

static bool RegisterWrapper(LPCWSTR native, LPCWSTR alias, WNDPROC& original)
{
    WNDCLASSW cls = {};
    if (!GetClassInfoW(nullptr, native, &cls)) return false;
    original = cls.lpfnWndProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = alias;
    cls.lpfnWndProc = InnoControlProc;
    return RegisterClassW(&cls) != 0;
}

static INT_PTR CALLBACK DialogProc(HWND dialog, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) {
        AddControl(dialog, L"Static", L"Neutral installer panel", 0, 20, 15, 300, 25, 1001);
        AddControl(dialog, L"Static", L"Colored panel is preserved", 0, 20, 55, 300, 25, 1002);
        HWND picture = AddControl(dialog, L"Static", L"", SS_BITMAP, 20, 95, 64, 32, 1003);
        SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(artwork));
        AddControl(dialog, L"InstallerFixturePainter", L"", 0, 130, 95, 190, 32, 1004);
        HWND license = AddControl(dialog, MSFTEDIT_CLASS, L"Formatted license remains app-owned.",
            ES_READONLY | ES_MULTILINE | WS_VSCROLL | WS_BORDER, 20, 145, 400, 55, 1005);
        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = RGB(0, 0, 128);
        SendMessageW(license, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
        SendMessageW(license, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
        AddControl(dialog, L"Button", L"Next", WS_TABSTOP | BS_DEFPUSHBUTTON, 330, 215, 90, 28, IDOK);
        AddControl(dialog, L"Edit", L"Native caret and selection", WS_TABSTOP | ES_AUTOHSCROLL,
            20, 215, 285, 25, 1006);
        if (!interactive) SetTimer(dialog, 1, 1000, nullptr);
        return TRUE;
    }
    if (msg == WM_CTLCOLORDLG || msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN ||
        msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        HWND child = reinterpret_cast<HWND>(lp);
        wchar_t cls[32] = {};
        GetClassNameW(child, cls, 32);
        if (engine == L"inno" && (!wcscmp(cls,L"TNewStaticText") ||
            !wcscmp(cls,L"TNewEdit") || !wcscmp(cls,L"TNewButton")))
            return SendMessageW(child, msg + 0xBC00, wp, lp);
        return FixtureColor(child, reinterpret_cast<HDC>(wp));
    }
    if (msg == WM_TIMER && wp == 1) {
        KillTimer(dialog, 1);
        RedrawWindow(dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        COLORREF sample[5] = {};
        const bool captured = SaveCapture(dialog, outputPrefix + L".bmp", sample);
        const auto dark = [](COLORREF c) { return GetRValue(c) < 100 && GetGValue(c) < 100 && GetBValue(c) < 100; };
        const bool neutralOkay = expectDark ? dark(sample[0]) && dark(sample[1])
            : sample[0] == RGB(245, 245, 245) && sample[1] == RGB(245, 245, 245);
        const bool preserved = sample[2] == RGB(229, 191, 120) &&
            sample[3] == RGB(180, 40, 210) && sample[4] == RGB(12, 90, 150);
        const bool licenseOkay = licenseBluePixels >= 40 && licenseLightBackground;
        result = captured && neutralOkay && preserved && licenseOkay ? 0 : 5;
        FILE* file = nullptr;
        _wfopen_s(&file, (outputPrefix + L".json").c_str(), L"wb");
        if (!file) result = 6;
        else {
            fprintf(file, "{\"expect_dark\":%s,\"captured\":%s,\"neutral_ok\":%s,\"artwork_preserved\":%s,\"license_text_preserved\":%s,\"license_blue_pixels\":%u,\"samples\":[%lu,%lu,%lu,%lu,%lu],\"exit_code\":%d}\n",
                expectDark ? "true" : "false", captured ? "true" : "false", neutralOkay ? "true" : "false",
                preserved ? "true" : "false", licenseOkay ? "true" : "false", licenseBluePixels,
                static_cast<unsigned long>(sample[0]), static_cast<unsigned long>(sample[1]),
                static_cast<unsigned long>(sample[2]), static_cast<unsigned long>(sample[3]),
                static_cast<unsigned long>(sample[4]), result);
            if (fclose(file)) result = 6;
        }
        if (!interactive) DestroyWindow(dialog);
        return TRUE;
    }
    if (msg == WM_COMMAND && (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)) {
        DestroyWindow(dialog);
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK EngineProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CREATE) { DialogProc(hwnd, WM_INITDIALOG, 0, 0); return 0; }
    if (msg == WM_ERASEBKGND && engine == L"inno") {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, neutralBrush);
        return 1;
    }
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
        return DialogProc(hwnd, msg, wp, lp);
    if (msg == WM_TIMER || msg == WM_COMMAND) return DialogProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd,msg,wp,lp);
}

#pragma pack(push, 2)
struct FixtureTemplate { DLGTEMPLATE dialog; WORD menu, windowClass, title; };
#pragma pack(pop)

int wmain(int argc, wchar_t** argv)
{
    if ((argc < 3 || argc > 5) || (wcscmp(argv[1], L"dark") && wcscmp(argv[1], L"baseline"))) {
        fwprintf(stderr, L"Usage: installer-fixture.exe dark|baseline OUTPUT_PREFIX [classic|inno|wix] [interactive]\n");
        return 2;
    }
    expectDark = wcscmp(argv[1], L"dark") == 0;
    outputPrefix = argv[2];
    if (argc >= 4) engine = argv[3];
    if (engine != L"classic" && engine != L"inno" && engine != L"wix") return 2;
    if (argc == 5) { if (wcscmp(argv[4], L"interactive")) return 2; interactive = true; }
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&controls) || !LoadLibraryW(L"Msftedit.dll")) return 3;
    neutralBrush = CreateSolidBrush(RGB(245, 245, 245));
    colorBrush = CreateSolidBrush(RGB(229, 191, 120));
    customBrush = CreateSolidBrush(RGB(12, 90, 150));
    fixtureFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Tahoma");
    std::vector<DWORD> image(64 * 32, 0x00B428D2); // BI_RGB is 0x00RRGGBB
    artwork = CreateBitmap(64, 32, 1, 32, image.data());
    WNDCLASSW klass = {};
    klass.lpfnWndProc = CustomPainter;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpszClassName = L"InstallerFixturePainter";
    if (!RegisterClassW(&klass) || !artwork) return 3;
    alignas(DWORD) FixtureTemplate description = {};
    description.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    description.dialog.cx = 300;
    description.dialog.cy = 180;
    // Child coordinates are pixels. Set a stable client area after creation.
    HWND dialog = nullptr;
    if (engine == L"classic") {
        dialog = CreateDialogIndirectParamW(GetModuleHandleW(nullptr), &description.dialog, nullptr, DialogProc, 0);
    } else {
        if (engine == L"inno" &&
            (!RegisterWrapper(L"Static",L"TNewStaticText",nativeStaticProc) ||
             !RegisterWrapper(L"Edit",L"TNewEdit",nativeEditProc) ||
             !RegisterWrapper(L"Button",L"TNewButton",nativeButtonProc))) return 3;
        WNDCLASSW root = {};
        root.hInstance = GetModuleHandleW(nullptr);
        root.lpfnWndProc = EngineProc;
        root.hbrBackground = neutralBrush;
        root.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        root.lpszClassName = engine == L"inno" ? L"TWizardForm" : L"WixStdBA";
        if (!RegisterClassW(&root)) return 3;
        dialog = CreateWindowExW(0,root.lpszClassName,L"Engine adapter fixture",
            WS_OVERLAPPEDWINDOW,100,100,440,260,nullptr,nullptr,root.hInstance,nullptr);
    }
    if (!dialog) return 3;
    RECT size = {0, 0, 440, 260};
    AdjustWindowRectEx(&size, description.dialog.style, FALSE, 0);
    SetWindowPos(dialog, nullptr, 100, 100, size.right - size.left, size.bottom - size.top, SWP_SHOWWINDOW);
    SetWindowTextW(dialog, L"Legacy installer fixture");
    MSG message;
    while (IsWindow(dialog) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (interactive && message.message == WM_KEYDOWN &&
            (message.wParam == VK_F5 || message.wParam == VK_F6)) {
            expectDark = message.wParam == VK_F6;
            const auto prefix = outputPrefix;
            outputPrefix += expectDark ? L"-dark" : L"-native";
            DialogProc(dialog, WM_TIMER, 1, 0);
            outputPrefix = prefix;
        } else if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    DeleteObject(fixtureFont);
    DeleteObject(artwork);
    DeleteObject(customBrush);
    DeleteObject(colorBrush);
    DeleteObject(neutralBrush);
    return result;
}
