static bool PaintNativePropertyTabBackground(HDC hdc, INT part, INT state, LPCRECT rect)
{
    if (!hdc || !rect || !g_settings.FlatPropertyPresentation || IsWinUiTabStyleActive() || !IsCustomDarkModeAllowed() ||
        SysHighContrastEnabled() || g_tabFeaturesUnloading.load(std::memory_order_acquire))
        return false;
    HWND tab = WindowFromDC(hdc);
    if (!IsClassName(tab, L"SysTabControl32")) tab = g_tlsPaintHwnd;
    if (!IsClassName(tab, L"SysTabControl32")) return false;
    HWND root = GetAncestor(tab, GA_ROOT);
    if (!IsClassName(root, L"#32770") || !GetPropW(root, kPropertyDialogDarkModeMarker) ||
        FindWindowExW(root, nullptr, L"SysTabControl32", nullptr) != tab ||
        (GetWindowLongPtrW(tab, GWL_STYLE) & (TCS_VERTICAL | TCS_BOTTOM)) ||
        ((GetWindowLongPtrW(tab,GWL_STYLE)&TCS_OWNERDRAWFIXED)&&!GetPropW(tab,kNativeFlatTabOwner)))
        return false;
    const bool pane = part == TABP_PANE || part == TABP_BODY;
    if (!pane && (part < TABP_TABITEM || part > TABP_TOPTABITEMBOTHEDGE)) return false;
    const bool selected = state == TIS_SELECTED || state == TIS_FOCUSED;
    COLORREF color = Win32MainDark;
    // Hover retains a restrained dark-theme surface; selection uses bold text.
    if(!pane && state==TIS_HOT)color=RGB(48,48,48);
    HBRUSH fill = GetCachedSolidBrush(color);
    if (!fill) return false;
    FillRect_orig(hdc, rect, fill);
    if (!pane && selected) {
        const int stroke = (std::max)(1, MulDiv(1, GetDpiForWindow(tab), 96));
        HBRUSH edge = GetCachedSolidBrush(Win32MainDark);
        if (edge) {
            RECT top{rect->left,rect->top,rect->right,rect->top+stroke};
            RECT left{rect->left,rect->top+stroke,rect->left+stroke,rect->bottom};
            RECT right{rect->right-stroke,rect->top+stroke,rect->right,rect->bottom};
            FillRect_orig(hdc,&top,edge);FillRect_orig(hdc,&left,edge);FillRect_orig(hdc,&right,edge);
        }
    }
    return true;
}