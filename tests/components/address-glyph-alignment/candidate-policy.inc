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

