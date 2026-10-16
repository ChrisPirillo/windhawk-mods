enum class InstallerEngine { None, Classic, Inno, WixStdBA };

static InstallerEngine InstallerRootEngine(LPCWSTR cls)
{
    if (!cls) return InstallerEngine::None;
    if (!_wcsicmp(cls, L"#32770")) return InstallerEngine::Classic;
    if (!_wcsicmp(cls, L"TWizardForm") || !_wcsicmp(cls, L"TUninstallProgressForm"))
        return InstallerEngine::Inno;
    if (!_wcsicmp(cls, L"WixStdBA")) return InstallerEngine::WixStdBA;
    return InstallerEngine::None;
}

// Map only verified VCL wrappers of native controls. No substring matching:
// TRichEditViewer, TBitmapImage and authored custom controls
// do not become Edit/Static/ListBox merely because their names look similar.
static LPCWSTR InstallerNativeClass(InstallerEngine engine, LPCWSTR cls)
{
    if (engine != InstallerEngine::Inno) return cls;
    if (!_wcsicmp(cls, L"TNewButton") || !_wcsicmp(cls, L"TNewCheckBox") ||
        !_wcsicmp(cls, L"TNewRadioButton")) return L"Button";
    if (!_wcsicmp(cls, L"TNewStaticText")) return L"Static";
    if (!_wcsicmp(cls, L"TNewEdit") || !_wcsicmp(cls, L"TNewPathEdit") ||
        !_wcsicmp(cls, L"TPasswordEdit") || !_wcsicmp(cls, L"TNewMemo")) return L"Edit";
    if (!_wcsicmp(cls, L"TNewListBox")) return L"ListBox";
    if (!_wcsicmp(cls, L"TNewComboBox")) return L"ComboBox";
    return cls;
}

// Typography may use this scope independently of the current color mode.
static InstallerEngine InstallerWindowEngine(HWND hwnd)
{
    const Settings* settings = GetSettings();
    if (!settings->InstallerUi || !IsCurrentProcessInteractive() || !hwnd)
        return InstallerEngine::None;
    HWND root = GetAncestor(hwnd, GA_ROOT);
    DWORD pid = 0;
    GetWindowThreadProcessId(root, &pid);
    wchar_t cls[64] = {};
    if (pid != GetCurrentProcessId() || !GetClassNameW(root, cls, ARRAYSIZE(cls)))
        return InstallerEngine::None;
    const auto engine = InstallerRootEngine(cls);
    // Unknown/custom Burn BAs are deliberately not inferred from an EXE name.
    if (engine == InstallerEngine::Classic && !settings->InstallerUiProcessTarget)
        return InstallerEngine::None;
    return engine;
}

static bool InstallerUiRoot(HWND hwnd)
{
    return InstallerWindowEngine(hwnd) != InstallerEngine::None;
}

