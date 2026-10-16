// Native hover, pressed and focus parts still belong to the control. Own only
// common backgrounds of the positively identified header, before the separate
// Explorer category gate. This changes no Explorer window or file-item paint.
static bool PaintFilePickerHeaderBackground(HDC hdc, const std::wstring& cls,
    INT part, INT state, LPCRECT rect)
{
    if (!hdc || !rect || state != 0)
        return false;
    const bool rebar = _wcsicmp(cls.c_str(), L"Rebar") == 0 &&
        (part == 0 || part == RP_BAND || part == RP_BACKGROUND);
    const bool common = part == 0 &&
        (cls == L"Toolbar" || cls == L"Navigation" ||
         cls == L"AddressBand" || cls == L"SearchBox");
    if ((!rebar && !common) ||
        !FilePickerHeaderChromeRoot(ResolvePaintHwndFreshFirst(hdc)))
        return false;
    const COLORREF color = FilePickerControlBackground(true);
    if (FilePickerMicaActive()) {
        const DWORD opaque = 0xFF000000u |
            (static_cast<DWORD>(GetRValue(color)) << 16) |
            (static_cast<DWORD>(GetGValue(color)) << 8) | GetBValue(color);
        WriteExplorerMicaPixels(hdc, *rect, opaque);
    } else if (HBRUSH brush = GetCachedSolidBrush(color)) {
        FillRect_orig(hdc, rect, brush);
    } else {
        return false;
    }
    return true;
}
