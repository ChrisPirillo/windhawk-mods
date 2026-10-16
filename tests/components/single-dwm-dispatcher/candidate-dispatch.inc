static HRESULT WINAPI DwmSetWindowAttribute_hook(
    HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
    // Windhawk owns one detour for this target. Keep all attribute policies
    // behind that registration, with one live original trampoline.
    if(dwAttribute==DWMWA_TRANSITIONS_FORCEDISABLED || dwAttribute==35 || dwAttribute==36)
        return OpeningDwmSetAttribute_hook(hWnd,dwAttribute,pvAttribute,cbAttribute);
    HRESULT hr;
    if (dwAttribute == DWMWA_WINDOW_CORNER_PREFERENCE &&
        MenuShouldOverrideDwmFrame(hWnd))
    {
        const DWORD corner = static_cast<DWORD>(
            g_settings.MenuCornerStyle);
        hr = DwmSetWindowAttribute_orig(
            hWnd, dwAttribute, &corner, sizeof(corner));
    }
    else if (dwAttribute == kDwmwaBorderColor &&
             MenuShouldOverrideDwmFrame(hWnd))
    {
        const COLORREF borderClr = MenuAcrylicDwmBorderColor(g_darkModeActive);
        hr = DwmSetWindowAttribute_orig(
            hWnd, dwAttribute, &borderClr, sizeof(borderClr));
    }
    else if (ExplorerMicaActive() &&
             !g_darkModeUnloading.load(std::memory_order_acquire) &&
             (dwAttribute == DWMWA_SYSTEMBACKDROP_TYPE ||
              dwAttribute == kDwmwaUseHostBackdropBrush) &&
             (IsClassName(hWnd, L"CabinetWClass") ||
              IsClassName(hWnd, L"ExploreWClass") ||
              (FilePickerMicaActive() && FilePickerRoot(hWnd) == hWnd)))
    {
        // Reactive, not one-shot: confirmed via a throwaway test mod that
        // Explorer itself natively calls this with
        // DWMWA_USE_HOSTBACKDROPBRUSH on its own CabinetWClass window
        // (likely tied to its own WinUI titlebar setup) -- a value only
        // pushed once from outside can get silently stomped by that later
        // call. Redirect whichever of the two attributes Explorer tried to
        // set to a real DWMWA_SYSTEMBACKDROP_TYPE Mica every time.
        MicaNativeRecord(hWnd, [&](MicaNativeFrame& frame) {
            if (!pvAttribute || cbAttribute < sizeof(DWORD))
                return;
            if (dwAttribute == DWMWA_SYSTEMBACKDROP_TYPE) {
                frame.haveBackdrop = true;
                frame.backdrop = *static_cast<const DWORD*>(pvAttribute);
            } else {
                frame.haveHostBrush = true;
                frame.hostBrush = *static_cast<const BOOL*>(pvAttribute);
            }
        });
        hr = DwmSetWindowAttribute_orig(hWnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                        &kDwmsbtMainWindow,
                                        sizeof(kDwmsbtMainWindow));
    }
    else
    {
        hr = DwmSetWindowAttribute_orig(
            hWnd, dwAttribute, pvAttribute, cbAttribute);
    }

    if (dwAttribute == DWMWA_USE_IMMERSIVE_DARK_MODE ||
        dwAttribute == 19 /* pre-20H1 DWMWA_USE_IMMERSIVE_DARK_MODE */)
        HwndDarkCacheInvalidate();

    // Some hosts' COMDLG32 reverts classic Open/Save dialogs back to light
    // DWM dark-mode state shortly after ApplyPropertyDialogDarkMode. Counter-apply only
    // on windows already tagged by us, and at most once per window, to avoid
    // unbounded synchronous write/revert ping-pong with DWM.
    if (dwAttribute == DWMWA_USE_IMMERSIVE_DARK_MODE &&
        pvAttribute && cbAttribute >= sizeof(BOOL) &&
        *reinterpret_cast<const BOOL*>(pvAttribute) == FALSE &&
        GetPropW(hWnd, kPropertyDialogDarkModeMarker) && IsSystemDarkMode() &&
        IsCustomDarkModeAllowed() &&
        !GetPropW(hWnd, L"_PropDkCounterApplied"))
    {
        SetPropW(hWnd, L"_PropDkCounterApplied", (HANDLE)1);
        const BOOL darkAttr = TRUE;
        DwmSetWindowAttribute_orig(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkAttr, sizeof(darkAttr));
    }

    return hr;
}
