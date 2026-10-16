static bool IsInsideShellFolderView(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return false;

    bool inDefView = false;
    for (HWND p = GetParent(hwnd); p; p = GetParent(p)) {
        wchar_t pc[64] = {};
        if (GetClassNameW(p, pc, ARRAYSIZE(pc)) &&
            _wcsicmp(pc, L"SHELLDLL_DefView") == 0) {
            inDefView = true;
            break;
        }
    }
    if (!inDefView)
        return false;

    HWND root = GetAncestor(hwnd, GA_ROOT);
    wchar_t rootCls[64] = {};
    if (!root || !GetClassNameW(root, rootCls, ARRAYSIZE(rootCls)))
        return false;

    if (_wcsicmp(rootCls, L"CabinetWClass") == 0 ||
        _wcsicmp(rootCls, L"ExploreWClass") == 0) {
        return true;
    }

    if (_wcsicmp(rootCls, L"#32770") != 0 &&
        _wcsicmp(rootCls, L"NativeHWNDHost") != 0) {
        return false;
    }

    // Both root classes are generic and can host unrelated UI. A real modern
    // shell file picker also owns a NamespaceTreeControl; do not cache an
    // early miss because DirectUI can paint before that tree finishes loading.
    return NavDividerRootContainsNamespaceTree(
        root, /*cacheNegative=*/false);
}
