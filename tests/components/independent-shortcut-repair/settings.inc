struct Settings {
    BOOL     ModernInsert       = TRUE;
    // Derived, not options of their own: RemoveTreeLines follows
    // ModernSeparators and AnimatedArrows follows RoundedButtons. Keeping
    // them as fields leaves every call site reading the thing it means, and
    // lets an override touch one without touching the other -- MMC drops
    // ModernSeparators but keeps its tree lines removed.
    BOOL     RemoveTreeLines    = TRUE;
    BOOL     AnimatedArrows     = TRUE;
    COLORREF InsertMarkClr      = RGB(0x6B, 0x6B, 0x6B);
    BOOL     InsertMarkUseAccent = TRUE;
    BOOL     GeneralSection     = TRUE;
    INT      ModernFocusRect    = 0; // 0=modern,1=hidden; GeneralSection off forces -1 (native passthrough)
    BOOL     ProgressBars       = FALSE;
    BOOL     AccentMarquee      = FALSE;
    BOOL     AccentColorize     = FALSE;
    BOOL     ExplorerSection    = TRUE;
    BOOL     NavPaneHoverFade   = FALSE;
    BOOL     NeutralSelection   = FALSE;
    INT      NavDividerMode     = 0; // 0=native, 1=remove, 2=hover reveal
    BOOL     RemoveNavDivider   = FALSE; // derived from NavDividerMode
    BOOL     NavDividerHoverReveal = FALSE; // derived
    BOOL     NavPaneWinUIMetrics = FALSE;
    BOOL     LegacyRebarControls = TRUE;         // derived from the ReBar option
    BOOL     RebarMicaTint      = FALSE;         // derived
    INT      ExplorerMicaBackdrop = 0; // 0=off, 1=full Mica, 2=Mica with content pane, 3=solid content pane, 4=opaque shades (no Mica)
    INT      StatusBarWidth     = 0; // 0=off, 1=beside the navigation pane, 2=same without the view-mode buttons
    BOOL     NavPanePill        = FALSE; // derived from the pill style option
    BOOL     NavPaneExpandAnimation = FALSE;
    INT      NavPillStyle       = 4; // 0=none,1=expand,2=fade,3=slide,4=winui_top
    BOOL     NavPillNoClip      = FALSE;
    BOOL     FluentPinIcon     = FALSE; // derived from the pin icon option
    int      PinIconStyle      = 0;  // 0=outline 1=filled, same option
    int      PinIconColor      = 0;  // 0=normal 1=accent 2=gradient
    int      PinMarginRight    = 0;
    INT      GlyphIconMode   = 0;  // 0=disabled, 1=static, 2=GPU, 3=native paint
    BOOL     GlyphIcons       = FALSE; // derived from GlyphIconMode
    BOOL     IconPopDirectComposition = FALSE; // derived from GlyphIconMode
    INT      ColorNavIconMode = 0;  // 0=disabled, 1=static, 2=animated
    BOOL     ColorNavIcons     = FALSE; // derived from ColorNavIconMode
    BOOL     ModernizeShellIcons = FALSE;
    BOOL     ModernShortcutOverlay = FALSE;
    BOOL     ShellTextDirectWrite = FALSE;
    BOOL     DialogTextDirectWrite = FALSE;
    BOOL     ShellTextRasterCache = FALSE; // derived: Explorer or regedit process
    COLORREF GlyphColor       = CLR_INVALID;
    BOOL     DiskChartAccentColor = FALSE;
    // DarkMode is now automatic — follows system dark mode
    BOOL     NormalizeDragDrop  = FALSE;
    BOOL     ModernTooltips     = TRUE;
    BOOL     ModernScrollbars      = TRUE;
    BOOL     ModernContextMenus = TRUE;
    INT      MenuCornerStyle    = 2;  // DWMWCP_ROUND
    INT      MenuHoverRadius    = static_cast<int>(WinUI::ControlCornerRadius); // DIPs
    BOOL     MenuWinUiMetrics   = TRUE;
    BOOL     ConsistentUIFonts = FALSE;
    BOOL     DisableWindowOpeningAnimations = FALSE;
    BOOL     RoundedButtons    = TRUE;
    BOOL     EditFocusLine     = TRUE;  // derived from RoundedButtons
    BOOL     EditBottomLine    = FALSE;
    BOOL     AccentGradients   = FALSE;
    BOOL     ModernSeparators  = TRUE;
    BOOL     RoundedTabPane   = TRUE;
    BOOL     SingleLinePropertyTabs = FALSE;
    BOOL     FlatPropertyPresentation = FALSE;
    BOOL     FlatLegacyBorders = FALSE;
    BOOL     OpeningAnimationProcessTarget = FALSE;
    BOOL     OpeningAnimationAllApplications = FALSE;
    BOOL     EnableDarkMode     = FALSE;
    BOOL     InstallerUi        = FALSE;
    bool     InstallerUiProcessTarget = false; // derived from exact executable names
    COLORREF CustomAccentColor = 0;
    bool     CustomAccentColorSet = false; // #000000 is a valid color
    // Dropdown popups follow ModernContextMenus; corner style comes from the
    // same MenuCornerStyle option.
    DWORD    CbCornerStyle = 3;          // 1=donotround 2=round 3=roundsmall
    // RegeditSection -- disabled by default, regedit.exe only
    BOOL     RegeditSection = FALSE;
    BOOL     RgMicaBackdrop = FALSE;    // TransparentBg = mica
    BOOL     RgGlyphIcons = FALSE;
    // WinverSection -- disabled by default, winver.exe only
    BOOL     WinverSection = FALSE;
    INT      WinverBackground = 0; // 0=solid, 1=mica, 2=micaAlt, 3=acrylic
};

// Settings are immutable after publication. Old snapshots stay alive until
// module teardown, so hot hooks need only one atomic pointer load and never
// contend with Wh_ModSettingsChanged or observe fields being overwritten.
static const Settings g_defaultSettings{};
static std::atomic<const Settings*> g_settingsCurrent{ &g_defaultSettings };
static std::mutex g_settingsPublishMutex;
static std::vector<std::unique_ptr<Settings>> g_settingsSnapshots;

static const Settings* GetSettings()
{
    return g_settingsCurrent.load(std::memory_order_acquire);
}

#define g_settings (*GetSettings())

