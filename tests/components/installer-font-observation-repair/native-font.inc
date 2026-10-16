// Native UI font policy. Controls retain native layout, selection and IME.
static bool InstallerUiRoot(HWND hwnd);
static constexpr wchar_t kNativeUiFontObserved[] = L"WH_NATIVE_UI_FONT_OBSERVED";
static constexpr wchar_t kNativeUiFontOriginal[] = L"WH_NATIVE_UI_FONT_ORIGINAL";
static constexpr wchar_t kNativeUiFontAssigned[] = L"WH_NATIVE_UI_FONT_ASSIGNED";
static std::atomic<bool> g_nativeUiFontUnloading{false};
static UINT g_nativeUiFontSyncMessage = 0;
static std::mutex g_nativeUiFontMutex;
struct NativeUiFontEntry { LOGFONTW description; HFONT font; };
static std::vector<NativeUiFontEntry> g_nativeUiFonts;

static bool NativeUiFontEnabled()
{
    return g_settings.GeneralSection && g_settings.ConsistentUIFonts &&
        !g_nativeUiFontUnloading.load(std::memory_order_acquire) &&
        !g_roundedSelectionHighContrast.load(std::memory_order_acquire) &&
        !IsTextPipelineDisabled();
}

static bool NativeUiFontScope(HWND hwnd)
{
    if (IsCurrentProcessRegedit())
        return true;
    if (InstallerUiRoot(hwnd))
        return true;
    HWND root = GetAncestor(hwnd, GA_ROOT);
    if (!IsClassName(root, L"#32770"))
        return false;
    // Advanced and Change Icon are owned dialogs, not property-sheet children.
    for (unsigned depth = 0; root && depth < 16; ++depth) {
        DWORD pid = 0;
        GetWindowThreadProcessId(root, &pid);
        if (pid != GetCurrentProcessId())
            return false;
        if (InstallerUiRoot(root) || (IsClassName(root, L"#32770") &&
                FindWindowExW(root, nullptr, L"SysTabControl32", nullptr)))
            return true;
        root = GetWindow(root, GW_OWNER);
    }
    return false;
}

static bool NativeUiFontPreserveAddressGlyphToolbar(HWND hwnd)
{
    if(!IsClassName(hwnd,L"ToolbarWindow32") ||
       !IsClassName(GetParent(hwnd),L"msctls_progress32"))return false;
    HWND root=GetAncestor(hwnd,GA_ROOT);
    if(!IsClassName(root,L"#32770"))return false;
    for(HWND parent=GetParent(hwnd);parent && parent!=root;parent=GetParent(parent))
        if(IsClassName(parent,L"ReBarWindow32"))return true;
    return false;
}

static bool NativeUiFontControl(HWND hwnd, LPCWSTR cls)
{
    if(NativeUiFontPreserveAddressGlyphToolbar(hwnd))return false;
    cls = InstallerNativeClass(InstallerWindowEngine(hwnd), cls);
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (!_wcsicmp(cls, L"Static")) {
        const auto type = style & SS_TYPEMASK;
        return type == SS_LEFT || type == SS_CENTER || type == SS_RIGHT ||
            type == SS_SIMPLE || type == SS_LEFTNOWORDWRAP;
    }
    if (!_wcsicmp(cls, L"Button"))
        return !(style & (BS_ICON | BS_BITMAP)) && (style & BS_TYPEMASK) != BS_OWNERDRAW;
    if (!_wcsicmp(cls, L"ListBox"))
        return !(style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE));
    if (!_wcsicmp(cls, L"ComboBox"))
        return !(style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE));
    if (!_wcsicmp(cls, L"SysListView32") && (style & LVS_OWNERDRAWFIXED))
        return false;
    constexpr LPCWSTR classes[] = {L"Edit", L"ComboBox", L"ComboLBox", L"SysLink",
        L"SysListView32", L"SysTreeView32", L"SysHeader32", L"SysTabControl32",
        L"msctls_hotkey32", L"msctls_statusbar32", L"ToolbarWindow32",
        L"NativeFontCtl", L"CHECKLIST_ACLUI"};
    for (const auto* candidate : classes)
        if (!_wcsicmp(cls, candidate))
            return true;
    return false;
}

static HFONT NativeUiFontResolve(HFONT original)
{
    LOGFONTW font = {};
    if (GetObjectW(original ? original : GetStockObject(DEFAULT_GUI_FONT),
            sizeof(font), &font) != sizeof(font) ||
        font.lfCharSet == SYMBOL_CHARSET || font.lfEscapement || font.lfOrientation ||
        (font.lfPitchAndFamily & 0xF0) == FF_MODERN)
        return original;
    // Explicit symbol faces sometimes use DEFAULT_CHARSET.
    constexpr LPCWSTR symbols[] = {L"Segoe Fluent Icons", L"Segoe MDL2 Assets",
        L"Segoe UI Symbol", L"Marlett", L"Symbol", L"Wingdings", L"Webdings"};
    for (const auto* symbol : symbols)
        if (!_wcsicmp(font.lfFaceName, symbol))
            return original;
    static const bool variableAvailable = FontFamilyExists(L"Segoe UI Variable Text");
    const auto* family = variableAvailable ? L"Segoe UI Variable Text" : L"Segoe UI";
    if (!_wcsicmp(font.lfFaceName, family))
        return original;
    wcsncpy_s(font.lfFaceName, family, _TRUNCATE);
    font.lfWidth = 0;
    font.lfQuality = CLEARTYPE_QUALITY;
    std::lock_guard<std::mutex> lock(g_nativeUiFontMutex);
    for (const auto& entry : g_nativeUiFonts)
        if (!memcmp(&entry.description, &font, sizeof(font)))
            return entry.font;
    // Never evict a font: a native control can retain its handle until unload.
    if (g_nativeUiFonts.size() >= 256)
        return original;
    HFONT replacement = CreateFontIndirectW(&font);
    if (replacement)
        g_nativeUiFonts.push_back({font, replacement});
    return replacement ? replacement : original;
}

static HFONT NativeUiFontOriginal(HWND hwnd)
{
    const HANDLE stored = GetPropW(hwnd, kNativeUiFontOriginal);
    return stored == reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-1))
        ? nullptr : reinterpret_cast<HFONT>(stored);
}

static void NativeUiFontRestore(HWND hwnd)
{
    if (GetPropW(hwnd, kNativeUiFontOriginal)) {
        DefSubclassProc(hwnd, WM_SETFONT,
            reinterpret_cast<WPARAM>(NativeUiFontOriginal(hwnd)), TRUE);
        RemovePropW(hwnd, kNativeUiFontOriginal);
        RemovePropW(hwnd, kNativeUiFontAssigned);
    }
}

static bool NativeUiFontIsOwned(HFONT font)
{
    std::lock_guard<std::mutex> lock(g_nativeUiFontMutex);
    for (const auto& entry : g_nativeUiFonts)
        if (entry.font == font)
            return true;
    return false;
}

static HFONT NativeUiFontApplicationFont(HWND hwnd, HFONT requested)
{
    if (!NativeUiFontIsOwned(requested))
        return requested;
    // List views and combos forward our assigned font to their children.
    // Restore the application's font, never a cache handle deleted on unload.
    for (HWND parent = GetParent(hwnd); parent; parent = GetParent(parent)) {
        if (GetPropW(parent, kNativeUiFontAssigned) == reinterpret_cast<HANDLE>(requested) &&
            GetPropW(parent, kNativeUiFontOriginal))
            return NativeUiFontOriginal(parent);
    }
    if (GetPropW(hwnd, kNativeUiFontOriginal))
        return NativeUiFontOriginal(hwnd);
    const HFONT current = reinterpret_cast<HFONT>(DefSubclassProc(hwnd, WM_GETFONT, 0, 0));
    // A child already inherited the replacement before it was observed.
    return NativeUiFontIsOwned(current) ? static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)) : current;
}

static WPARAM NativeUiFontSelect(HWND hwnd, HFONT requested)
{
    const HFONT original = NativeUiFontApplicationFont(hwnd, requested);
    const HFONT replacement = NativeUiFontResolve(requested);
    const HANDLE saved = original ? reinterpret_cast<HANDLE>(original)
        : reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-1));
    if (!SetPropW(hwnd, kNativeUiFontOriginal, saved))
        return reinterpret_cast<WPARAM>(original);
    if (!SetPropW(hwnd, kNativeUiFontAssigned, reinterpret_cast<HANDLE>(replacement))) {
        RemovePropW(hwnd, kNativeUiFontOriginal);
        return reinterpret_cast<WPARAM>(original);
    }
    return reinterpret_cast<WPARAM>(replacement);
}

static void RestoreNativePropertyTabFont(HWND);
static HFONT NativePropertyTabUnderlyingFont(HWND,HFONT);
static LRESULT CALLBACK NativeUiFontSubclass(HWND hwnd, UINT msg, WPARAM wp,
    LPARAM lp, DWORD_PTR)
{
    if (msg == WM_NCDESTROY) {
        RemovePropW(hwnd, kNativeUiFontObserved);
        RemovePropW(hwnd, kNativeUiFontOriginal);
        RemovePropW(hwnd, kNativeUiFontAssigned);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, NativeUiFontSubclass);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    wchar_t cls[32] = {};
    if(msg==WM_SETFONT && t_nativePropertyFontUpdate)
        return DefSubclassProc(hwnd,msg,wp,lp);
    if(msg==WM_SETFONT || msg==g_nativeUiFontSyncMessage)
        RestoreNativePropertyTabFont(hwnd);
    const bool synchronize = msg == g_nativeUiFontSyncMessage;
    const bool fontEvent = msg == WM_SETFONT;
    const bool refresh = synchronize || msg == WM_PAINT || msg == WM_SHOWWINDOW ||
        msg == WM_DPICHANGED_AFTERPARENT || msg == WM_THEMECHANGED ||
        msg == WM_STYLECHANGED || msg == WM_SETTINGCHANGE;
    if (fontEvent || refresh) {
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        const bool enabled = NativeUiFontEnabled() && NativeUiFontScope(hwnd) &&
            NativeUiFontControl(hwnd, cls);
        if (fontEvent) {
            if (enabled) {
                const HFONT assigned = reinterpret_cast<HFONT>(
                    GetPropW(hwnd, kNativeUiFontAssigned));
                const HFONT requested = reinterpret_cast<HFONT>(wp);
                if (!assigned || assigned != requested)
                    wp = NativeUiFontSelect(hwnd, requested);
            } else {
                // The new application font supersedes the previous original.
                RemovePropW(hwnd, kNativeUiFontOriginal);
                RemovePropW(hwnd, kNativeUiFontAssigned);
            }
        } else if (!enabled) {
            NativeUiFontRestore(hwnd);
        } else {
            const HFONT current = NativePropertyTabUnderlyingFont(hwnd,
                reinterpret_cast<HFONT>(DefSubclassProc(hwnd, WM_GETFONT, 0, 0)));
            const HFONT assigned = reinterpret_cast<HFONT>(
                GetPropW(hwnd, kNativeUiFontAssigned));
            if (!GetPropW(hwnd, kNativeUiFontOriginal) || current != assigned) {
                const WPARAM replacement = NativeUiFontSelect(hwnd, current);
                if (reinterpret_cast<HFONT>(replacement) != current)
                    DefSubclassProc(hwnd, WM_SETFONT, replacement, TRUE);
            }
        }
        if (synchronize) {
            if (wp || !g_settings.ConsistentUIFonts ||
                g_nativeUiFontUnloading.load(std::memory_order_acquire)) {
                NativeUiFontRestore(hwnd);
                RemovePropW(hwnd, kNativeUiFontObserved);
                WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, NativeUiFontSubclass);
            }
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void NativeUiFontObserve(HWND hwnd, LPCWSTR cls)
{
    if (!NativeUiFontEnabled() || !g_nativeUiFontSyncMessage ||
        !NativeUiFontControl(hwnd, cls))
        return;
    DWORD pid = 0;
    const DWORD thread = GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId())
        return;
    // Dialog children can be created before the property sheet's tab control.
    // Observe their final font assignments and recheck scope before any change.
    if (!IsCurrentProcessRegedit() &&
        !IsClassName(GetAncestor(hwnd, GA_ROOT), L"#32770") &&
        !InstallerUiRoot(hwnd))
        return;
    if (thread != GetCurrentThreadId() && !NativeUiFontScope(hwnd))
        return;
    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, NativeUiFontSubclass, 0)) {
        if (!SetPropW(hwnd, kNativeUiFontObserved, reinterpret_cast<HANDLE>(1))) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, NativeUiFontSubclass);
            return;
        }
        if (g_nativeUiFontUnloading.load(std::memory_order_acquire)) {
            SendMessageW(hwnd, g_nativeUiFontSyncMessage, TRUE, 0);
        } else if (thread == GetCurrentThreadId())
            SendMessageW(hwnd, g_nativeUiFontSyncMessage, 0, 0);
        else
            PostMessageW(hwnd, g_nativeUiFontSyncMessage, 0, 0);
    }
}

static BOOL CALLBACK NativeUiFontRefreshChild(HWND hwnd, LPARAM remove)
{
    const bool stop = remove || !NativeUiFontEnabled();
    if (g_nativeUiFontSyncMessage && GetPropW(hwnd, kNativeUiFontObserved)) {
        // Existing controls must recompute scope even when a profile was just
        // disabled and would now decline observation. Restore synchronously
        // before any replacement HFONT can be released.
        SendMessageW(hwnd, g_nativeUiFontSyncMessage, stop, 0);
    } else if (!stop) {
        wchar_t cls[32] = {};
        if (GetClassNameW(hwnd, cls, ARRAYSIZE(cls)))
            NativeUiFontObserve(hwnd, cls);
    }
    return TRUE;
}

static BOOL CALLBACK NativeUiFontRefreshTop(HWND hwnd, LPARAM remove)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) {
        NativeUiFontRefreshChild(hwnd, remove);
        EnumChildWindows(hwnd, NativeUiFontRefreshChild, remove);
    }
    return TRUE;
}

static void NativeUiFontRefresh(bool remove = false)
{
    EnumWindows(NativeUiFontRefreshTop, remove);
}

static void NativeUiFontRelease()
{
    std::lock_guard<std::mutex> lock(g_nativeUiFontMutex);
    for (const auto& entry : g_nativeUiFonts)
        DeleteObject(entry.font);
    g_nativeUiFonts.clear();
}


