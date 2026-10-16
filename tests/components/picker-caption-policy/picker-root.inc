static HWND ClassicPickerEraseRoot(HWND hwnd)
{
    HWND root = hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
    if (root && FilePickerDialogRoot(hwnd)) return root;
    if (!root || !IsClassName(root, L"#32770") ||
        FindWindowExW(root, nullptr, L"DUIViewWndClassName", nullptr) ||
        !IsClassName(GetDlgItem(root, 1121), L"SHELLDLL_DefView") ||
        !IsClassName(GetDlgItem(root, 1136), L"ComboBox") ||
        !IsClassName(GetDlgItem(root, 1137), L"ComboBox"))
        return nullptr;
    return root;
}


