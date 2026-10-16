static std::mutex                       g_cbtHooksMutex;
// Per thread ID, with the creation time of the thread that ID belonged to: a
// thread that exits takes its hook with it, and a new thread can reuse the ID.
struct CbtThreadHook {
    HHOOK hook = nullptr;
    FILETIME created = {};
};
static std::unordered_map<DWORD, CbtThreadHook> g_cbtHooks;
// The calling thread already carries its hook: the common case, checked for
// free on every call. InitCbtHooks_Cleanup (unload, or both features turned
// off) bumps the generation, so a flag set before it no longer counts.
static thread_local bool t_cbtHookInstalled = false;
static thread_local unsigned t_cbtHookGeneration = 0;
static std::atomic<unsigned> g_cbtHooksGeneration{0};

// Callback for EnumChildWindows — applies dark mode to children of flyout windows



// WH_CBT rather than WH_CALLWNDPROC: a WH_CALLWNDPROC hook on a thread makes
// user32 route every same-thread SendMessage through the kernel instead of
// calling the window procedure directly (~2 us instead of ~0.06 us per call,
// in every process with a top-level window), and both users of this hook only
// need window creation. HCBT_CREATEWND also fires for the #32768 popups the
// kernel creates inside TrackPopupMenu, which NtUserCreateWindowEx_hook never
// sees. The window exists at that point but hasn't received WM_NCCREATE yet.
static void ConsistentCaptionObserveNewWindow(HWND,const CREATESTRUCTW*);
static HRESULT ConsistentCaptionDwmSet(HWND,DWORD,LPCVOID,DWORD);
static bool ConsistentCaptionTrackingEnabled(){
    return g_settings.EnableDarkMode||g_settings.RegeditSection||g_settings.InstallerUi;
}
static void OpeningTransitionObserveNewWindow(HWND,const CREATESTRUCTW*);
static void DialogPresentationObserveNewWindow(HWND,const CREATESTRUCTW*);
static LRESULT CALLBACK CbtProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HCBT_CREATEWND)
    {
        const HWND hwnd = reinterpret_cast<HWND>(wParam);
        const CREATESTRUCTW* cs =
            reinterpret_cast<const CBT_CREATEWNDW*>(lParam)->lpcs;
        wchar_t cls[32] = {};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));

        ConsistentCaptionObserveNewWindow(hwnd,cs);
        OpeningTransitionObserveNewWindow(hwnd,cs);
        if (!g_isXamlProcess)
            DialogPresentationObserveNewWindow(hwnd,cs);

        // ── Acrylic context menus ──────────────────────────────────────────
        // Everything MenuTryInstallAcrylic does (DWM attributes, composition
        // attribute, subclass) only needs the HWND; MN_SIZEWINDOW re-applies
        // once the popup has its size.
        if (g_settings.ModernContextMenus
            && (_wcsicmp(cls, MENUPOPUP_CLASS) == 0
                || _wcsicmp(cls, L"DropDown") == 0
                || _wcsicmp(cls, L"ViewControlClass") == 0)) {
            MenuTryInstallAcrylic(hwnd);
        }

        // ── Checkbox animation subclass ────────────────────────────────────
        // The check state isn't known yet (dialogs set it after creation);
        // CheckBoxSubclassProc reads it on the first WM_PAINT.
        if (!g_isXamlProcess && g_settings.RoundedButtons && cs && _wcsicmp(cls, L"Button") == 0 &&
            !g_checkBoxAnimUnloading.load(std::memory_order_acquire))
        {
            const LONG style = cs->style & 0xFL;
            const bool isChk = (style == BS_CHECKBOX   || style == BS_AUTOCHECKBOX ||
                                style == BS_3STATE     || style == BS_AUTO3STATE);
            if (isChk)
            {
                bool hasSubclass;
                {
                    std::lock_guard<std::recursive_mutex> lk(g_checkAnimsMutex);
                    hasSubclass = g_checkAnims.find((ULONG_PTR)hwnd) != g_checkAnims.end();
                }
                if (!hasSubclass &&
                    WindhawkUtils::SetWindowSubclassFromAnyThread(
                        hwnd, CheckBoxSubclassProc, 0))
                {
                    CheckAnim ca;
                    ca.fromState = -1;
                    ca.progress  = 1.f;
                    ca.lastTick  = GetTickCount();
                    std::lock_guard<std::recursive_mutex> lk(g_checkAnimsMutex);
                    g_checkAnims[(ULONG_PTR)hwnd] = ca;
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

