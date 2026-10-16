static bool IsInsideDesktop(HWND hFrom)
{
    HWND root = GetAncestor(hFrom, GA_ROOT);
    if (!root) return false;
    wchar_t cls[64] = {};
    GetClassNameW(root, cls, ARRAYSIZE(cls));
    return (_wcsicmp(cls, L"Progman") == 0 ||
            _wcsicmp(cls, L"WorkerW") == 0);
}
