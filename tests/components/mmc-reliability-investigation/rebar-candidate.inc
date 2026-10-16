static void DrawRebarBandLine(HDC hdc, LPCRECT pRect)
{
    // The rebar background is already filled by both theme paths. The flat
    // legacy-border option must not add a replacement rule over that fill.
    if (g_settings.FlatLegacyBorders && IsCurrentProcessMMC() &&
        g_darkModeActive && IsCustomDarkModeAllowed() && !SysHighContrastEnabled())
        return;
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(48, 48, 48));
    if (!hPen) return;
    HGDIOBJ old = SelectObject(hdc, hPen);
    MoveToEx(hdc, pRect->left, pRect->bottom - 1, nullptr);
    LineTo(hdc, pRect->right, pRect->bottom - 1);
    SelectObject(hdc, old);
    DeleteObject(hPen);
}
