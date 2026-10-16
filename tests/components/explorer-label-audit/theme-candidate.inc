static bool NativeExplorerItemListTheme(HWND hwnd)
{
    if(!IsCurrentProcessExplorer() || !IsClassName(hwnd,L"SysListView32"))return false;
    return (IsInsideExplorer(hwnd) && IsInsideShellFolderView(hwnd)) ||
        (IsInsideDesktop(hwnd) && IsClassName(GetParent(hwnd),L"SHELLDLL_DefView"));
}
HRESULT WINAPI SetWindowTheme_hook(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)
{
    if(NativeExplorerItemListTheme(hwnd)){
        const HRESULT result=SetWindowTheme_orig(hwnd,pszSubAppName,pszSubIdList);
        if(SUCCEEDED(result))RemovePropW(hwnd,kAppliedThemeProp);
        return result;
    }
    // Early exit for themes we apply repeatedly from several paths. Skipping
    // identical SetWindowTheme calls avoids duplicate WM_THEMECHANGED cascades
    // while leaving DrawTheme/custom-draw hooks untouched.
    if (GetKnownThemeSignature(pszSubAppName, pszSubIdList)) {
        return SetKnownWindowThemeOnce(hwnd, pszSubAppName, pszSubIdList);
    }

    // Redirect ListView/TreeView to Explorer-family themes. In light mode the
    // plain Explorer theme gives the modern hover/selection metrics without
    // changing colors; in dark Explorer we keep DarkMode_Explorer. Scoped to
    // IsCurrentProcessExplorer() only -- forcing this on a third-party
    // process's own ListView/TreeView (e.g. Revo Uninstaller Pro) caused a
    // SetWindowTheme/WM_THEMECHANGED reentrancy hang there.
    if (hwnd && !g_darkModeExcluded && IsCurrentProcessExplorer()) {
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        if (_wcsicmp(cls, L"SysListView32") == 0 ||
            _wcsicmp(cls, L"SysTreeView32") == 0) {
            return SetExplorerFamilyTheme(hwnd, pszSubIdList);
        }
    }

    HRESULT hr = SetWindowTheme_orig(hwnd, pszSubAppName, pszSubIdList);
    if (hwnd && SUCCEEDED(hr))
        RemovePropW(hwnd, kAppliedThemeProp);
    return hr;
}
