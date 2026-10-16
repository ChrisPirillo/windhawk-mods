// Native property-sheet fixture. Run on Windows with the candidate loaded.
// F5 checks modern families; F6 checks native restoration; F7 changes an edit's
// font after creation. Switch pages before checking to exercise lazy creation.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#include <richedit.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dwmapi.h>

struct Sample {
    HWND window;
    std::wstring name;
    LOGFONTW original;
    bool modern;
    HFONT originalHandle;
};
static std::vector<Sample> samples;
static std::vector<HFONT> appFonts;
static HFONT bodyFont, symbolFont, monoFont;
static HWND editable;
static HWND sheet;
static FILE* evidence;
static unsigned checkNumber;
static bool automatic;
static bool expectModern;
static unsigned automaticFailures;
static unsigned creationFailures;
static HWND richEdit;
static CHARFORMAT2W richExpected{};
static bool lifecycle;
static unsigned lifecycleStage;
static ULONGLONG lifecycleStarted;

static void Log(const std::wstring& text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    std::fputs(utf8.c_str(), evidence);
    std::fputc('\n', evidence);
    std::fflush(evidence);
    std::fputs(utf8.c_str(), stdout);
    std::fputc('\n', stdout);
}

static HFONT MakeFont(LPCWSTR family, LONG height, LONG weight, BYTE charset = DEFAULT_CHARSET,
                      BYTE pitch = DEFAULT_PITCH) {
    LOGFONTW font{};
    font.lfHeight = height;
    font.lfWeight = weight;
    font.lfCharSet = charset;
    font.lfPitchAndFamily = pitch;
    wcsncpy_s(font.lfFaceName, family, _TRUNCATE);
    HFONT handle = CreateFontIndirectW(&font);
    if (handle) appFonts.push_back(handle);
    return handle;
}

static HWND Add(HWND page, LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y,
                int w, int h, LPCWSTR name, HFONT font = nullptr, bool modern = true) {
    const UINT dpi = GetDpiForWindow(page);
    HWND hwnd = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        MulDiv(x, dpi, 96), MulDiv(y, dpi, 96), MulDiv(w, dpi, 96), MulDiv(h, dpi, 96),
        page, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) { ++creationFailures; Log(L"FAIL create " + std::wstring(name)); return nullptr; }
    HFONT requested = font ? font : bodyFont;
    LOGFONTW original{};
    GetObjectW(requested, sizeof(original), &original);
    samples.push_back({hwnd, name, original, modern, requested});
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(requested), TRUE);
    if (!_wcsicmp(cls, MSFTEDIT_CLASS)) {
        // RichEdit owns document formats; WM_GETFONT need not return an HFONT.
        richEdit = hwnd;
        richExpected.cbSize = sizeof(richExpected);
        richExpected.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT | CFM_COLOR;
        wcscpy_s(richExpected.szFaceName, original.lfFaceName);
        richExpected.yHeight = MulDiv(abs(original.lfHeight), 1440, dpi);
        richExpected.wWeight = static_cast<WORD>(original.lfWeight);
        richExpected.crTextColor = RGB(0, 0, 128);
        if (!SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_ALL,
                reinterpret_cast<LPARAM>(&richExpected))) {
            ++creationFailures;
            Log(L"FAIL assign RichEdit document format");
        }
        // This authored navy document needs its authored light surface too;
        // custom dark mode can change the inherited system COLOR_WINDOW.
        SendMessageW(hwnd, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
    }
    return hwnd;
}

static bool NativeInputCheck() {
    if (!editable) return false;
    SetWindowTextW(editable, L"Alpha beta");
    SendMessageW(editable, EM_SETSEL, 6, 10);
    SendMessageW(editable, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"gamma"));
    wchar_t text[64]{};
    GetWindowTextW(editable, text, ARRAYSIZE(text));
    DWORD first = 0, last = 0;
    SendMessageW(editable, EM_GETSEL, reinterpret_cast<WPARAM>(&first),
                 reinterpret_cast<LPARAM>(&last));
    bool good = wcscmp(text, L"Alpha gamma") == 0 && first == 11 && last == 11;
    // Character positions and selection remain native after font substitution.
    for (int i = 0; i < 11; ++i) {
        LRESULT position = SendMessageW(editable, EM_POSFROMCHAR, i, 0);
        if (position == -1) { good = false; break; }
        LRESULT index = SendMessageW(editable, EM_CHARFROMPOS, 0, position);
        if (LOWORD(index) != i) good = false;
    }
    Log(good ? L"PASS native edit text, selection, character positions"
             : L"FAIL native edit text, selection, character positions");
    return good;
}

static unsigned Check(bool modern) {
    ++checkNumber;
    unsigned failed = creationFailures;
    Log(L"CHECK " + std::to_wstring(checkNumber) +
        (modern ? L" EXPECT MODERN" : L" EXPECT NATIVE RESTORED"));
    for (const auto& sample : samples) {
        if (!IsWindow(sample.window)) {
            ++failed;
            Log(L"FAIL missing control " + sample.name);
            continue;
        }
        if (sample.window == richEdit) {
            CHARFORMAT2W actual{};
            actual.cbSize = sizeof(actual);
            SendMessageW(richEdit, EM_GETCHARFORMAT, SCF_SELECTION,
                reinterpret_cast<LPARAM>(&actual));
            const DWORD mask = richExpected.dwMask;
            RECT rc{};
            GetClientRect(richEdit, &rc);
            HDC screen = GetDC(richEdit);
            HDC memory = CreateCompatibleDC(screen);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = rc.right;
            info.bmiHeader.biHeight = -rc.bottom;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            HGDIOBJ old = bitmap ? SelectObject(memory, bitmap) : nullptr;
            bool readable = bitmap && bits && BitBlt(memory, 0, 0, rc.right, rc.bottom, screen, 0, 0, SRCCOPY);
            unsigned blue = 0;
            if (readable) {
                const auto* pixels = static_cast<const RGBQUAD*>(bits);
                for (int y = 0; y < rc.bottom; ++y)
                    for (int x = 0; x < rc.right; ++x) {
                        const auto& p = pixels[y * rc.right + x];
                        if (p.rgbBlue > p.rgbRed + 40 && p.rgbBlue > p.rgbGreen + 40) ++blue;
                    }
                const auto& bg = pixels[(rc.bottom - 8) * rc.right + rc.right - 8];
                readable = blue >= 40 && bg.rgbRed >= 240 && bg.rgbGreen >= 240 && bg.rgbBlue >= 240;
                const std::string path = "rich-document-check-" + std::to_string(checkNumber) + ".bmp";
                FILE* image = std::fopen(path.c_str(), "wb");
                if (image) {
                    BITMAPFILEHEADER header{};
                    header.bfType = 0x4D42;
                    header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
                    const DWORD bytes = rc.right * rc.bottom * 4;
                    header.bfSize = header.bfOffBits + bytes;
                    std::fwrite(&header, sizeof(header), 1, image);
                    std::fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, image);
                    std::fwrite(bits, bytes, 1, image);
                    std::fclose(image);
                }
            }
            if (old) SelectObject(memory, old);
            if (bitmap) DeleteObject(bitmap);
            if (memory) DeleteDC(memory);
            if (screen) ReleaseDC(richEdit, screen);
            const bool good = readable && (actual.dwMask & mask) == mask &&
                !_wcsicmp(actual.szFaceName, richExpected.szFaceName) &&
                actual.yHeight == richExpected.yHeight &&
                actual.wWeight == richExpected.wWeight &&
                !(actual.dwEffects & CFE_AUTOCOLOR) &&
                actual.crTextColor == richExpected.crTextColor;
            if (!good) ++failed;
            Log(std::wstring(good ? L"PASS " : L"FAIL ") +
                L"RichEdit document face/size/weight/color and readable blue-on-white pixels preserved");
            continue;
        }
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(sample.window, WM_GETFONT, 0, 0));
        LOGFONTW actual{};
        bool good = font && GetObjectW(font, sizeof(actual), &actual) == sizeof(actual);
        wchar_t face[LF_FACESIZE]{};
        HDC dc = GetDC(sample.window);
        if (dc) {
            HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
            GetTextFaceW(dc, ARRAYSIZE(face), face);
            if (old) SelectObject(dc, old);
            ReleaseDC(sample.window, dc);
        } else good = false;
        if (modern && sample.modern) {
            bool family = !_wcsnicmp(face, L"Segoe UI Variable", 17) || !_wcsicmp(face, L"Segoe UI");
            good = good && family && actual.lfHeight == sample.original.lfHeight &&
                actual.lfWeight == sample.original.lfWeight;
        } else {
            good = good && font == sample.originalHandle &&
                std::memcmp(&actual, &sample.original, sizeof(actual)) == 0;
        }
        if (!good) ++failed;
        Log(std::wstring(good ? L"PASS " : L"FAIL ") + sample.name + L" actual=" + face +
            L" requested=" + actual.lfFaceName + L" height=" + std::to_wstring(actual.lfHeight) +
            L" weight=" + std::to_wstring(actual.lfWeight) + L" original=" + sample.original.lfFaceName);
    }
    if (!NativeInputCheck()) ++failed;
    if (lifecycle && lifecycleStage == 2 && modern) {
        // The same root/page existed before this reinjection. Font recovery
        // alone is insufficient: its dialog subclasses and palette must return.
        BOOL darkTitle=FALSE;
        const HRESULT titleResult=DwmGetWindowAttribute(sheet,20,&darkTitle,sizeof(darkTitle));
        HWND page=PropSheet_GetCurrentPageHwnd(sheet);
        RECT rc{};GetClientRect(page,&rc);
        HDC screen=GetDC(page),memory=screen?CreateCompatibleDC(screen):nullptr;
        HBITMAP bitmap=memory?CreateCompatibleBitmap(screen,rc.right,rc.bottom):nullptr;
        HGDIOBJ old=bitmap?SelectObject(memory,bitmap):nullptr;
        bool printed=bitmap && PrintWindow(page,memory,PW_CLIENTONLY);
        COLORREF background=printed?GetPixel(memory,rc.right-8,rc.bottom-8):CLR_INVALID;
        bool restored=GetPropW(sheet,L"_PropDkApplied") && GetPropW(page,L"_PropDkApplied") &&
            SUCCEEDED(titleResult) && darkTitle && background==RGB(32,32,32);
        Log(std::wstring(restored?L"PASS ":L"FAIL ")+L"same-window dark dialog/pane restored after reinjection; title="+
            std::to_wstring(darkTitle)+L" page_background="+std::to_wstring(background));
        if(!restored)++failed;
        if(old)SelectObject(memory,old);if(bitmap)DeleteObject(bitmap);
        if(memory)DeleteDC(memory);if(screen)ReleaseDC(page,screen);
    }
    if (samples.size() < 13) {
        ++failed;
        Log(L"FAIL second property page has not been opened; open Lazy page and recheck");
    }
    Log(L"RESULT failures=" + std::to_wstring(failed));
    if (!automatic)
        MessageBoxW(sheet, failed ? L"Check failed. See modernizer-fixture-results.txt."
                                : L"Check passed. See modernizer-fixture-results.txt.",
                    L"Font fixture", MB_OK | (failed ? MB_ICONERROR : MB_ICONINFORMATION));
    return failed;
}

static void CALLBACK AutomaticCheck(HWND hwnd, UINT, UINT_PTR timer, DWORD) {
    if (lifecycle) {
        if(FILE* abort=std::fopen("lifecycle-abort.txt","rb")){
            std::fclose(abort);Log(L"FAIL lifecycle aborted by controller");
            ++automaticFailures;KillTimer(hwnd,timer);DestroyWindow(hwnd);return;
        }
        if (GetTickCount64() - lifecycleStarted > 120000) {
            Log(L"FAIL lifecycle timed out");
            ++automaticFailures;
            KillTimer(hwnd, timer);
            DestroyWindow(hwnd);
            return;
        }
        if (lifecycleStage) {
            const std::string marker = "lifecycle-go-" + std::to_string(lifecycleStage) + ".txt";
            FILE* signal = std::fopen(marker.c_str(), "rb");
            if (!signal) return;
            std::fclose(signal);
        }
        automaticFailures += Check(lifecycleStage == 0 || lifecycleStage == 2);
        FILE* phase = std::fopen("lifecycle-phase.txt", "wb");
        if (phase) { std::fprintf(phase, "%u", lifecycleStage); std::fclose(phase); }
        if (++lifecycleStage < 4) return;
    } else {
        automaticFailures = Check(expectModern);
    }
    KillTimer(hwnd, timer);
    DestroyWindow(hwnd);
}

static INT_PTR CALLBACK PageProc(HWND page, UINT msg, WPARAM, LPARAM lp) {
    if (msg != WM_INITDIALOG) return FALSE;
    auto* info = reinterpret_cast<PROPSHEETPAGEW*>(lp);
    if (info->lParam == 2) {
        Add(page, L"Static", L"Created only when this page is opened", 0, 12, 15, 410, 25, L"lazy label");
        Add(page, L"Edit", L"Lazy editable field", WS_BORDER | ES_AUTOHSCROLL, 12, 50, 400, 26, L"lazy edit");
        HWND list = Add(page, WC_LISTVIEWW, L"", WS_BORDER | LVS_REPORT,
            12, 95, 230, 110, L"lazy list");
        LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH};
        column.pszText = const_cast<LPWSTR>(L"Native header");
        column.cx = MulDiv(210, GetDpiForWindow(page), 96);
        ListView_InsertColumn(list, 0, &column);
        LVITEMW item{LVIF_TEXT};
        item.pszText = const_cast<LPWSTR>(L"A native list value");
        ListView_InsertItem(list, &item);
        HWND header = ListView_GetHeader(list);
        LOGFONTW original{};
        GetObjectW(bodyFont, sizeof(original), &original);
        HFONT priorHeader = MakeFont(L"Tahoma", -16, FW_BOLD);
        if (!priorHeader) ++creationFailures;
        SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(priorHeader), TRUE);
        // A list view can forward the mod-owned replacement handle to its
        // header; restoration must recover the parent's app-owned handle.
        HFONT forwarded = reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0));
        samples.push_back({header, L"lazy forwarded header", original, true, bodyFont});
        SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(forwarded), TRUE);
        HWND tree = Add(page, WC_TREEVIEWW, L"", WS_BORDER | TVS_HASLINES,
            255, 95, 205, 110, L"lazy tree");
        TVINSERTSTRUCTW node{};
        node.hParent = TVI_ROOT;
        node.hInsertAfter = TVI_LAST;
        node.item.mask = TVIF_TEXT;
        node.item.pszText = const_cast<LPWSTR>(L"A native tree item");
        TreeView_InsertItem(tree, &node);
        return TRUE;
    }
    Add(page, L"Static", L"Tahoma label with enough room for the replacement font", 0,
        12, 8, 450, 23, L"label");
    editable = Add(page, L"Edit", L"Alpha beta", WS_BORDER | ES_AUTOHSCROLL,
        12, 36, 330, 25, L"editable");
    Add(page, L"Edit", L"Read-only native edit", WS_BORDER | ES_READONLY,
        12, 67, 330, 25, L"readonly");
    Add(page, L"Edit", L"password", WS_BORDER | ES_PASSWORD,
        350, 36, 120, 25, L"password");
    Add(page, L"Button", L"A checkbox caption", BS_AUTOCHECKBOX | WS_TABSTOP,
        12, 98, 210, 25, L"checkbox");
    Add(page, L"Button", L"A normal button", BS_PUSHBUTTON | WS_TABSTOP,
        240, 98, 160, 27, L"button");
    HWND combo = Add(page, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        12, 135, 220, 90, L"combo");
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"First item"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Second item"));
    SendMessageW(combo, CB_SETCURSEL, 1, 0);
    Add(page, L"Static", L"ab", 0, 250, 140, 80, 25, L"symbol unchanged", symbolFont, false);
    Add(page, L"Static", L"Monospace unchanged", 0, 12, 180, 300, 25,
        L"monospace unchanged", monoFont, false);
    Add(page, MSFTEDIT_CLASS, L"Document text remains Tahoma", WS_BORDER | ES_MULTILINE,
        12, 215, 450, 60, L"RichEdit unchanged", bodyFont, false);
    Add(page, L"SysLink", L"<a>Native link caption</a>", WS_TABSTOP,
        12, 285, 250, 25, L"link");
    Add(page, L"Static", L"F5: modern   F6: restored   F7: late bold edit font\nOpen Lazy page before checks. Disable the option before F6.",
        0, 12, 325, 460, 50, L"instructions");
    return TRUE;
}

static std::vector<WORD> PageTemplate() {
    // A standard, zero-control DS_SETFONT page; all children are real controls.
    DLGTEMPLATE page{};
    page.style = WS_CHILD | WS_VISIBLE | DS_CONTROL | DS_SETFONT;
    page.cx = 330;
    page.cy = 260;
    std::vector<WORD> words((sizeof(page) + 1) / 2);
    std::memcpy(words.data(), &page, sizeof(page));
    words.insert(words.end(), {0, 0, 0, 9}); // menu, class, title, point size
    for (wchar_t c : std::wstring(L"Tahoma")) words.push_back(c);
    words.push_back(0);
    return words;
}

static void LateEditFont() {
    HFONT late = MakeFont(L"Tahoma", -18, FW_BOLD);
    if (!late) { ++creationFailures; Log(L"FAIL create late font"); return; }
    for (auto& sample : samples)
        if (sample.window == editable) {
            GetObjectW(late, sizeof(sample.original), &sample.original);
            sample.originalHandle = late;
        }
    SendMessageW(editable, WM_SETFONT, reinterpret_cast<WPARAM>(late), TRUE);
    Log(L"Sent late Tahoma bold -18 WM_SETFONT to editable; F5 then disable and F6.");
}

int main(int argc, char** argv) {
    if (argc > 1) {
        automatic = true;
        if (std::strcmp(argv[1], "--lifecycle") == 0) lifecycle = expectModern = true;
        else if (std::strcmp(argv[1], "--expect-modern") == 0) expectModern = true;
        else if (std::strcmp(argv[1], "--expect-native") != 0) return 2;
    }
    evidence = std::fopen("modernizer-fixture-results.txt", "wb");
    if (!evidence) return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES | ICC_LINK_CLASS};
    InitCommonControlsEx(&controls);
    HMODULE rich = LoadLibraryW(L"Msftedit.dll");
    if (!rich) { Log(L"FAIL Msftedit.dll"); return 2; }
    bodyFont = MakeFont(L"Tahoma", -MulDiv(9, GetDpiForSystem(), 72), FW_NORMAL);
    symbolFont = MakeFont(L"Marlett", -18, FW_NORMAL, SYMBOL_CHARSET);
    monoFont = MakeFont(L"Courier New", -16, FW_NORMAL, DEFAULT_CHARSET, FIXED_PITCH | FF_MODERN);
    if (!bodyFont || !symbolFont || !monoFont) return 2;
    auto pageTemplate = PageTemplate();
    PROPSHEETPAGEW pages[2]{};
    for (int i = 0; i < 2; ++i) {
        pages[i].dwSize = sizeof(pages[i]);
        pages[i].dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
        pages[i].hInstance = GetModuleHandleW(nullptr);
        pages[i].pResource = reinterpret_cast<DLGTEMPLATE*>(pageTemplate.data());
        pages[i].pszTitle = i ? L"Lazy page" : L"Native controls";
        pages[i].pfnDlgProc = PageProc;
        pages[i].lParam = i + 1;
    }
    PROPSHEETHEADERW header{};
    header.dwSize = sizeof(header);
    header.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_NOAPPLYNOW;
    header.hInstance = GetModuleHandleW(nullptr);
    header.pszCaption = L"Modernizer native font fixture";
    header.nPages = 2;
    header.ppsp = pages;
    sheet = reinterpret_cast<HWND>(PropertySheetW(&header));
    if (!sheet || sheet == reinterpret_cast<HWND>(-1)) return 2;
    Log(L"Fixture ready. F5 modern; F6 native restored; F7 late font; open Lazy page first.");
    if (automatic) {
        PropSheet_SetCurSel(sheet, nullptr, 1);
        PropSheet_SetCurSel(sheet, nullptr, 0);
        LateEditFont();
        lifecycleStarted = GetTickCount64();
        if (!SetTimer(sheet, 1, 750, AutomaticCheck)) return 2;
    }
    MSG msg{};
    while (IsWindow(sheet) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam >= VK_F5 && msg.wParam <= VK_F7) {
            if (msg.wParam == VK_F5) Check(true);
            else if (msg.wParam == VK_F6) Check(false);
            else if (editable) LateEditFont();
            continue;
        }
        if (!PropSheet_IsDialogMessage(sheet, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (!PropSheet_GetCurrentPageHwnd(sheet)) DestroyWindow(sheet);
    }
    if (IsWindow(sheet)) DestroyWindow(sheet);
    for (HFONT font : appFonts) DeleteObject(font);
    FreeLibrary(rich);
    Log(L"Fixture closed. Output assertions do not replace visual, IME or accessibility review.");
    std::fclose(evidence);
    return automaticFailures ? 1 : 0;
}

