static bool IsInsideExplorer(HWND hFrom)
{
    HWND root = GetAncestor(hFrom, GA_ROOT);
    if (!root) return false;
    wchar_t cls[64] = {};
    GetClassNameW(root, cls, ARRAYSIZE(cls));
    return (_wcsicmp(cls, L"CabinetWClass") == 0 ||
            _wcsicmp(cls, L"ExploreWClass") == 0);
}
