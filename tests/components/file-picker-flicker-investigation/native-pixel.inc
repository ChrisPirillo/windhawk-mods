static void StretchPremultipliedPixel(HDC hdc, const RECT& rect, DWORD bgra,
    bool blend)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top)
        return;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = 1;
    bi.bmiHeader.biHeight = 1;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp)
        return;
    if (bits)
        *static_cast<DWORD*>(bits) = bgra;
    if (HDC mem = CreateCompatibleDC(hdc)) {
        HGDIOBJ old = SelectObject(mem, bmp);
        const int w = rect.right - rect.left, h = rect.bottom - rect.top;
        if (blend) {
            const BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            AlphaBlend(hdc, rect.left, rect.top, w, h, mem, 0, 0, 1, 1, bf);
        } else {
            StretchBlt(hdc, rect.left, rect.top, w, h, mem, 0, 0, 1, 1, SRCCOPY);
        }
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    DeleteObject(bmp);
}

static void WriteExplorerMicaPixels(HDC hdc, const RECT& line, DWORD bgra)
{
    StretchPremultipliedPixel(hdc, line, bgra, false);
}


