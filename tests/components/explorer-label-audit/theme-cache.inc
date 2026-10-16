static int GetKnownThemeId(LPCWSTR pszSubAppName)
{
    if (!pszSubAppName) return 1;
    if (pszSubAppName[0] == L'\0') return 2;
    if (_wcsicmp(pszSubAppName, L"Explorer") == 0) return 3;
    if (_wcsicmp(pszSubAppName, L"DarkMode_Explorer") == 0) return 4;
    if (_wcsicmp(pszSubAppName, L"DarkMode_CFD") == 0) return 5;
    if (_wcsicmp(pszSubAppName, L"DarkMode_ItemsView") == 0) return 6;
    return 0;
}

static int GetKnownSubId(LPCWSTR pszSubIdList)
{
    if (!pszSubIdList) return 1;
    if (pszSubIdList[0] == L'\0') return 2;
    return 0;
}

static UINT_PTR GetKnownThemeSignature(LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)
{
    int themeId = GetKnownThemeId(pszSubAppName);
    int subId = GetKnownSubId(pszSubIdList);
    if (!themeId || !subId) return 0;
    return ((UINT_PTR)themeId << 8) | (UINT_PTR)subId;
}

static bool IsKnownWindowThemeApplied(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)
{
    if (!hwnd) return false;
    UINT_PTR sig = GetKnownThemeSignature(pszSubAppName, pszSubIdList);
    return sig && (UINT_PTR)GetPropW(hwnd, kAppliedThemeProp) == sig;
}

static HRESULT SetKnownWindowThemeOnce(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)
{
    UINT_PTR sig = GetKnownThemeSignature(pszSubAppName, pszSubIdList);
    if (!hwnd)
        return SetWindowTheme_orig(hwnd, pszSubAppName, pszSubIdList);

    if (IsKnownWindowThemeApplied(hwnd, pszSubAppName, pszSubIdList))
        return S_OK;

    HRESULT hr = SetWindowTheme_orig(hwnd, pszSubAppName, pszSubIdList);
    if (SUCCEEDED(hr)) {
        if (sig)
            SetPropW(hwnd, kAppliedThemeProp, (HANDLE)sig);
        else
            RemovePropW(hwnd, kAppliedThemeProp);
    }
    return hr;
}

static HRESULT SetExplorerFamilyTheme(HWND hwnd, LPCWSTR pszSubIdList)
{
    // Outside Explorer we don't own this window's theming -- GetWindowTheme
    // returns non-NULL only once SetWindowTheme has actually been called on
    // it (by us or by the app itself; a control that simply renders with
    // ambient manifest visual styles but never had SetWindowTheme called on
    // it still reports NULL here). If the app's own skinning engine already
    // asserted a theme, leave it alone instead of overriding it.
    if (!IsCurrentProcessExplorer() && hwnd && GetWindowTheme(hwnd) != nullptr)
        return S_FALSE;

    return SetKnownWindowThemeOnce(
        hwnd,
        (g_darkModeActive && IsCurrentProcessExplorer()) ? L"DarkMode_Explorer" : L"Explorer",
        pszSubIdList);
}

