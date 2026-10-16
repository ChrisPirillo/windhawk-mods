// Separate internal owner identity: generic menu/installer caption refreshes
// must not restore picker assignments, or vice versa. No extra HWND marker or
// lifetime map is needed; the caption owner owns those already.
static constexpr unsigned char kFilePickerCaptionCaller = 0;
static void FilePickerCaptionRefresh(HWND hwnd, UINT message)
{
    if (message != WM_SHOWWINDOW && message != WM_WINDOWPOSCHANGING &&
        message != WM_PAINT && message != WM_ACTIVATE &&
        message != WM_THEMECHANGED && message != WM_SETTINGCHANGE &&
        message != WM_NCDESTROY)
        return;
    const bool apply = message != WM_NCDESTROY &&
        ClassicPickerEraseRoot(hwnd) == hwnd && g_darkModeActive &&
        IsCustomDarkModeAllowed() &&
        !g_darkModeUnloading.load(std::memory_order_acquire);
    ConsistentCaptionAttributeForOwner(hwnd, 35, apply,
        FilePickerControlBackground(true), &kFilePickerCaptionCaller);
    ConsistentCaptionAttributeForOwner(hwnd, 36, apply,
        WinUI::Dark::TextFillColorPrimary, &kFilePickerCaptionCaller);
}
