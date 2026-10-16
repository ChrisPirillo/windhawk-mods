static Settings LoadSettings()
{
    Settings next{};
    next.GeneralSection   = Wh_GetIntSetting(L"GeneralSection.Enabled");
    next.DisableWindowOpeningAnimations = next.GeneralSection &&
        Wh_GetIntSetting(L"GeneralSection.DisableWindowOpeningAnimations");
    next.ConsistentUIFonts = next.GeneralSection &&
        Wh_GetIntSetting(L"GeneralSection.ConsistentUIFonts");
    next.ModernInsert     = Wh_GetIntSetting(L"GeneralSection.ModernInsertMark");
    {
        auto s = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"GeneralSection.InsertMarkColor"));
        next.InsertMarkUseAccent = _wcsicmp(s, L"accent") == 0;
        if (!next.InsertMarkUseAccent && wcslen(s) == 6) {
            DWORD v = wcstoul(s, nullptr, 16);
            next.InsertMarkClr = RGB((v>>16)&0xFF, (v>>8)&0xFF, v&0xFF);
        }
    }
    {
        // ModernFocusRect is a string enum: "modern"=0, "hidden"=1
        auto sv = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"GeneralSection.ModernFocusRect"));
        const wchar_t* sp = sv;
        if (wcscmp(sp, L"modern") == 0) next.ModernFocusRect = 0;
        else if (wcscmp(sp, L"native") == 0) next.ModernFocusRect = -1;
        else if (wcscmp(sp, L"hidden") == 0)    next.ModernFocusRect = 1;
        else                                     next.ModernFocusRect = 0;
    }
    next.ProgressBars     = Wh_GetIntSetting(L"GeneralSection.ProgressBars");
    next.NormalizeDragDrop = Wh_GetIntSetting(L"GeneralSection.NormalizeDragDrop");
    next.ModernTooltips    = Wh_GetIntSetting(L"GeneralSection.ModernTooltips");
    next.ModernScrollbars = Wh_GetIntSetting(L"GeneralSection.ModernScrollbars");
    {
        next.ModernContextMenus = FALSE;
        next.MenuWinUiMetrics = FALSE;
        auto mode = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"GeneralSection.ModernContextMenus"));
        if (wcscmp(mode, L"winui") == 0 || wcscmp(mode, L"native") == 0) {
            next.ModernContextMenus = TRUE;
            next.MenuWinUiMetrics = wcscmp(mode, L"winui") == 0;
        } else if (wcscmp(mode, L"off") != 0) {
            // Numeric values keep the former checkbox and its saved metrics.
            next.ModernContextMenus = wcscmp(mode, L"1") == 0 ||
                _wcsicmp(mode, L"true") == 0;
            auto metrics = WindhawkUtils::StringSetting(
                Wh_GetStringSetting(L"GeneralSection.MenuItemMetrics"));
            next.MenuWinUiMetrics = wcscmp(metrics, L"native") != 0;
        }
    }
    {
        auto style = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"GeneralSection.MenuCornerStyle"));
        // One option drives both surfaces; "default" keeps the Windows 11 look
        // of round menus over small-round dropdowns.
        if      (wcscmp(style, L"round")  == 0) { next.MenuCornerStyle = 2; next.CbCornerStyle = 2; }
        else if (wcscmp(style, L"smallround") == 0) { next.MenuCornerStyle = 3; next.CbCornerStyle = 3; }
        else if (wcscmp(style, L"square") == 0) { next.MenuCornerStyle = 1; next.CbCornerStyle = 1; }
        else                                    { next.MenuCornerStyle = 2; next.CbCornerStyle = 3; }
    }

    {
        wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,ARRAYSIZE(path));
        const wchar_t* name=wcsrchr(path,L'\\');
        auto targets=WindhawkUtils::StringSetting(Wh_GetStringSetting(L"GeneralSection.OpeningAnimationProcesses"));
        next.OpeningAnimationProcessTarget=InstallerProcessMatches(name?name+1:path,targets.get());
        auto scope=WindhawkUtils::StringSetting(Wh_GetStringSetting(L"GeneralSection.OpeningAnimationScope"));
        next.OpeningAnimationAllApplications=wcscmp(scope,L"application_windows")==0;
    }
    next.MenuHoverRadius    = Wh_GetIntSetting(L"GeneralSection.MenuHoverRadius");
    next.RoundedButtons   = Wh_GetIntSetting(L"GeneralSection.RoundedButtons");
    next.AccentGradients  = Wh_GetIntSetting(L"GeneralSection.AccentGradients");
    next.ModernSeparators = Wh_GetIntSetting(L"GeneralSection.ModernSeparators");
    next.FlatLegacyBorders = next.GeneralSection && Wh_GetIntSetting(L"GeneralSection.FlatLegacyBorders");
    {
        next.RoundedTabPane = FALSE;
        auto mode = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"GeneralSection.RoundedTabPane"));
        next.FlatPropertyPresentation = next.GeneralSection && wcscmp(mode,L"flat")==0;
        if (wcscmp(mode, L"winui") == 0 || wcscmp(mode, L"winui_single") == 0) {
            next.RoundedTabPane = TRUE;
            next.SingleLinePropertyTabs = wcscmp(mode, L"winui_single") == 0;
        } else if (wcscmp(mode,L"flat")==0) {
            next.SingleLinePropertyTabs = TRUE;
        } else if (wcscmp(mode, L"native") != 0) {
            next.RoundedTabPane = wcscmp(mode, L"1") == 0 ||
                _wcsicmp(mode, L"true") == 0;
            next.SingleLinePropertyTabs = next.RoundedTabPane &&
                Wh_GetIntSetting(L"GeneralSection.SingleLinePropertyTabs");
        }
    }
    next.EnableDarkMode      = Wh_GetIntSetting(L"GeneralSection.EnableDarkMode");
    next.InstallerUi = Wh_GetIntSetting(L"GeneralSection.InstallerUi");
    {
        wchar_t processPath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, processPath, ARRAYSIZE(processPath));
        const wchar_t* name = wcsrchr(processPath, L'\\');
        auto targets = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"GeneralSection.InstallerUiProcesses"));
        next.InstallerUiProcessTarget = InstallerProcessMatches(
            name ? name + 1 : processPath, targets.get());
    }
    // DarkMode is automatic (follows system)
    {
        auto hex = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"GeneralSection.CustomAccentColor"));
        next.CustomAccentColor = 0;
        next.CustomAccentColorSet = false;
        if (hex[0] == L'#' && wcslen(hex) == 7) {
            unsigned r = 0, g = 0, b = 0;
            if (swscanf(hex + 1, L"%02x%02x%02x", &r, &g, &b) == 3) {
                next.CustomAccentColor = RGB(r, g, b);
                next.CustomAccentColorSet = true;
            }
        }
    }
    next.NavPaneHoverFade = Wh_GetIntSetting(L"ExplorerSection.NavPaneHoverFade");
    next.NeutralSelection = Wh_GetIntSetting(L"ExplorerSection.NeutralSelection");

    next.ExplorerSection      = Wh_GetIntSetting(L"ExplorerSection.Enabled");
    {
        auto mode = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.AccentColorize"));
        if (wcscmp(mode, L"off") == 0 || wcscmp(mode, L"marquee") == 0 ||
            wcscmp(mode, L"interface") == 0 || wcscmp(mode, L"all") == 0) {
            next.AccentColorize = wcscmp(mode, L"interface") == 0 ||
                wcscmp(mode, L"all") == 0;
            next.AccentMarquee = wcscmp(mode, L"marquee") == 0 ||
                wcscmp(mode, L"all") == 0;
        } else {
            next.AccentColorize = wcscmp(mode, L"1") == 0 ||
                _wcsicmp(mode, L"true") == 0;
            next.AccentMarquee = Wh_GetIntSetting(L"ExplorerSection.AccentMarquee");
        }
    }
    {
        auto sv = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.NavDivider"));
        if      (wcscmp(sv, L"remove") == 0 ||
                 wcscmp(sv, L"removestrict") == 0) next.NavDividerMode = 1;
        else if (wcscmp(sv, L"hoverreveal")  == 0) next.NavDividerMode = 2;
        else                                       next.NavDividerMode = 0;
        // Stored strict removal now uses the normal pane-aware erase path.
        // Removal and hover reveal remain mutually exclusive.
        next.RemoveNavDivider      = next.NavDividerMode == 1;
        next.NavDividerHoverReveal = next.NavDividerMode == 2;
    }
    next.NavPaneWinUIMetrics  = Wh_GetIntSetting(L"ExplorerSection.NavPaneWinUIMetrics");
    // The fake-Mica tint is only reachable while ReBar styling is enabled.
    {
        auto sv = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.LegacyRebarControls"));
        next.LegacyRebarControls = wcscmp(sv, L"off") != 0;
        next.RebarMicaTint = wcscmp(sv, L"tinted") == 0;
    }
    {
        // ExplorerMicaBackdrop: "off"=0, "full"=1, "pane"=2, "solid"=3,
        // "opaque"=4; the former checkbox's stored 1/TRUE keeps meaning full.
        auto sv = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"ExplorerSection.ExplorerMicaBackdrop"));
        const wchar_t* sp = sv;
        if      (wcscmp(sp, L"pane") == 0) next.ExplorerMicaBackdrop = 2;
        else if (wcscmp(sp, L"solid") == 0) next.ExplorerMicaBackdrop = 3;
        else if (wcscmp(sp, L"opaque") == 0) next.ExplorerMicaBackdrop = 4;
        else if (wcscmp(sp, L"full") == 0 || wcscmp(sp, L"1") == 0 ||
                 _wcsicmp(sp, L"true") == 0) next.ExplorerMicaBackdrop = 1;
        else                               next.ExplorerMicaBackdrop = 0;
    }
    {
        auto sv = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.StatusBarWidth"));
        next.StatusBarWidth = wcscmp(sv, L"nobuttons") == 0 ? 2
                            : wcscmp(sv, L"on") == 0        ? 1
                                                            : 0;
    }
    // One option carries both "is there a pill" and "how does it move";
    // style 0 keeps its old meaning of a pill that doesn't animate.
    {
        auto sv = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.NavPanePill"));
        next.NavPanePill = wcscmp(sv, L"off") != 0;
        if      (wcscmp(sv, L"expand")     == 0) next.NavPillStyle = 1;
        else if (wcscmp(sv, L"fade")       == 0) next.NavPillStyle = 2;
        else if (wcscmp(sv, L"slide")      == 0) next.NavPillStyle = 3;
        else if (wcscmp(sv, L"winui_top")  == 0) next.NavPillStyle = 4;
        else                                     next.NavPillStyle = 0;
    }
    next.NavPaneExpandAnimation =
        Wh_GetIntSetting(L"ExplorerSection.NavPaneExpandAnimation");
    next.NavPillNoClip        = Wh_GetIntSetting(L"ExplorerSection.NavPillNoClip");
    {
        auto pinSty = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.FluentPinIcon"));
        next.FluentPinIcon = wcscmp(pinSty, L"disabled") != 0;
        next.PinIconStyle = (wcscmp(pinSty, L"filled") == 0) ? 1 : 0;
    }
    {
        auto pinClr = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"ExplorerSection.PinIconColor"));
        next.PinIconColor = (wcscmp(pinClr, L"gradient") == 0) ? 2
            : (wcscmp(pinClr, L"accent") == 0) ? 1 : 0;
    }
    next.PinMarginRight = std::clamp(Wh_GetIntSetting(L"ExplorerSection.PinMarginRight"), 0, 32);
    {
        auto mode = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.GlyphIcons"));
        const wchar_t* value = mode;
        if (wcscmp(value, L"static") == 0)
            next.GlyphIconMode = 1;
        else if (wcscmp(value, L"gpu") == 0)
            next.GlyphIconMode = 2;
        else if (wcscmp(value, L"native") == 0)
            next.GlyphIconMode = 3;
        else
            next.GlyphIconMode = 0;
        next.GlyphIcons = next.GlyphIconMode != 0;
        next.IconPopDirectComposition = next.GlyphIconMode == 2;
    }
    {
        auto colorMode = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"ExplorerSection.ColorNavIcons"));
        const wchar_t* cvalue = colorMode;
        if (wcscmp(cvalue, L"static") == 0)
            next.ColorNavIconMode = 1;
        else if (wcscmp(cvalue, L"animated") == 0)
            next.ColorNavIconMode = 2;
        else
            next.ColorNavIconMode = 0;
        next.ColorNavIcons = next.ColorNavIconMode != 0;
    }
    next.ModernizeShellIcons = Wh_GetIntSetting(L"ExplorerSection.ModernizeShellIcons");
    next.ModernShortcutOverlay =
        Wh_GetIntSetting(L"ExplorerSection.ModernShortcutOverlay");
    next.ShellTextDirectWrite =
        Wh_GetIntSetting(L"ExplorerSection.ShellTextDirectWrite");
    next.DialogTextDirectWrite = IsCurrentProcessExplorer() &&
        (next.ConsistentUIFonts ||
         Wh_GetIntSetting(L"ExplorerSection.DialogTextDirectWrite"));
    {
        auto s = WindhawkUtils::StringSetting(Wh_GetStringSetting(L"ExplorerSection.GlyphColor"));
        next.GlyphColor = CLR_INVALID;
        if (s[0] == L'#' && wcslen(s) == 7) {
            unsigned r, g, b;
            if (swscanf_s(s + 1, L"%02x%02x%02x", &r, &g, &b) == 3)
                next.GlyphColor = RGB(r, g, b);
        }
    }
    next.DiskChartAccentColor = Wh_GetIntSetting(L"ExplorerSection.DiskChartAccentColor");

    // Invalidate animation-enabled cache after reload
    next.RegeditSection = Wh_GetIntSetting(L"RegeditSection.Enabled");
    {
        auto sv = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"RegeditSection.TransparentBg"));
        const wchar_t* sp = sv;
        next.RgMicaBackdrop = wcscmp(sp, L"mica") == 0;
    }
    next.RgGlyphIcons = Wh_GetIntSetting(L"RegeditSection.GlyphIcons");
    next.WinverSection = Wh_GetIntSetting(L"WinverSection.Enabled");
    {
        auto bg = WindhawkUtils::StringSetting(
            Wh_GetStringSetting(L"WinverSection.Background"));
        if      (wcscmp(bg, L"mica")    == 0) next.WinverBackground = 1;
        else if (wcscmp(bg, L"micaAlt") == 0) next.WinverBackground = 2;
        else if (wcscmp(bg, L"acrylic") == 0) next.WinverBackground = 3;
        else                                  next.WinverBackground = 0;
    }

    // Section switches produce an effective snapshot. Hooks don't need to
    // repeat parent gates, and disabling a section takes effect consistently.
    if (!next.GeneralSection) {
        next.ModernInsert = FALSE;
        next.ModernFocusRect = -1; // native passthrough, not "hidden"
        next.ProgressBars = FALSE;
        next.NormalizeDragDrop = FALSE;
        next.ModernTooltips = FALSE;
        next.ModernScrollbars = FALSE;
        next.ModernContextMenus = FALSE;
        next.RoundedButtons = FALSE;
        next.EditFocusLine = FALSE;
        next.AccentGradients = FALSE;
        next.ModernSeparators = FALSE;
        next.RoundedTabPane = FALSE;
        next.SingleLinePropertyTabs = FALSE;
        next.EnableDarkMode = FALSE;
        next.InstallerUi = FALSE;
        next.CustomAccentColor = 0;
        next.CustomAccentColorSet = false;
    }

    // Settings that ride along with a broader one. Derived after the section
    // gate so they inherit it, and before the MMC override below so that
    // override keeps its narrow meaning.
    next.EditFocusLine = next.RoundedButtons;
    next.EditBottomLine = next.EditFocusLine &&
        Wh_GetIntSetting(L"GeneralSection.EditBottomLine");
    next.AnimatedArrows = next.RoundedButtons;
    next.RemoveTreeLines = next.ModernSeparators;

    if (!next.ExplorerSection) {
        next.NavPaneHoverFade = FALSE;
        next.NeutralSelection = FALSE;
        next.AccentColorize = FALSE;
        next.AccentMarquee = FALSE;
        next.NavDividerMode = 0;
        next.RemoveNavDivider = FALSE;
        next.NavDividerHoverReveal = FALSE;
        next.NavPaneWinUIMetrics = FALSE;
        next.LegacyRebarControls = FALSE;
        next.RebarMicaTint = FALSE;
        next.ExplorerMicaBackdrop = 0;
        next.StatusBarWidth = 0;
        next.NavPanePill = FALSE;
        next.NavPillStyle = 0;
        next.NavPillNoClip = FALSE;
        next.FluentPinIcon = FALSE;
        next.GlyphIconMode = 0;
        next.GlyphIcons = FALSE;
        next.IconPopDirectComposition = FALSE;
        next.ColorNavIconMode = 0;
        next.ColorNavIcons = FALSE;
        next.ModernizeShellIcons = FALSE;
        // Shortcut arrows retain their own persisted switch independently.
        next.ShellTextDirectWrite = FALSE;
        next.DialogTextDirectWrite = FALSE;
        next.DiskChartAccentColor = FALSE;
    }
    if (!next.RegeditSection) {
        next.RgMicaBackdrop = FALSE;
        next.RgGlyphIcons = FALSE;
    }

    // Explorer's DirectWrite label routes (folder view, desktop, navigation
    // pane) and regedit's tree keep their finished rasters; each route is
    // still enabled by its own option, and file pickers and other processes
    // draw directly.
    next.ShellTextRasterCache = IsCurrentProcessExplorer() || IsCurrentProcessRegedit();

    // Root-caused: the group box border is what crashes/mis-styles Computer
    // Management, Local Group Policy Editor, Performance Monitor, TPM
    // Management, and Hyper-V Manager -- those snap-ins are heavy with
    // property-sheet GroupBox/Static/DirectUI folder-view borders, exactly
    // what this feature touches. Force it off in MMC regardless of the
    // user's setting; every call site below reads next.ModernSeparators (or
    // the published g_settings copy of it), so this one override covers
    // all of them.
    if (IsCurrentProcessMMC()) {
        next.ModernSeparators = FALSE;
    }

    Settings previous = *GetSettings();
    auto snapshot = std::make_unique<Settings>(next);
    const Settings* published = snapshot.get();
    {
        std::lock_guard<std::mutex> lock(g_settingsPublishMutex);
        g_settingsSnapshots.emplace_back(std::move(snapshot));
        g_settingsCurrent.store(published, std::memory_order_release);
    }
    g_animEnabled.store(-1, std::memory_order_release);
    return previous;
}

