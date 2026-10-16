// Only the common picker's native navigation/address/search band. The shell
// view host and namespace tree are never chrome, even if a future template
// nests them below a WorkerW or ReBarWindow32.
static HWND FilePickerHeaderChromeRoot(HWND hwnd)
{
    if (!g_darkModeActive || !hwnd)
        return nullptr;
    HWND root = FilePickerRoot(hwnd);
    if (!root)
        return nullptr;
    HWND viewHost = FindWindowExW(root, nullptr, L"DUIViewWndClassName", nullptr);
    RECT view{};
    if (!viewHost || !GetWindowRect(viewHost, &view))
        return nullptr;
    HWND band = nullptr;
    for (HWND w = hwnd; w && w != root; w = GetParent(w)) {
        if (IsClassName(w, L"DUIViewWndClassName") ||
            IsClassName(w, L"SHELLDLL_DefView") ||
            IsClassName(w, L"NamespaceTreeControl"))
            return nullptr;
        if (IsClassName(w, L"ReBarWindow32") || IsClassName(w, L"WorkerW"))
            band = w;
    }
    RECT top{};
    if (!band || !GetWindowRect(band, &top) || top.bottom <= top.top ||
        top.bottom > view.top)
        return nullptr;
    return root;
}
