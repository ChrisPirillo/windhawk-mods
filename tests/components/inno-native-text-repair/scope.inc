// Known Inno wrappers paint ordinary native captions. A fresh glow/DIB pass
// is unnecessary there and can bring a pale opaque cell back over the adapter's
// dark surface. Keep native shaping/metrics and normalize only that paint's
// neutral text/background colors. Never enter RichEdit or arbitrary VCL paint.
static HWND InstallerNativeCaptionOwner(HDC dc){
    HWND owner=t_installerPaintWindow;
    if(!dc||!owner||!IsCustomDarkModeAllowed()||SysHighContrastEnabled()||
       InstallerWindowEngine(owner)!=InstallerEngine::Inno)return nullptr;
    HWND direct=WindowFromDC(dc);
    if(direct&&direct!=owner)return nullptr;
    wchar_t cls[64]{};if(!GetClassNameW(owner,cls,64))return nullptr;
    if(_wcsicmp(cls,L"TNewStaticText")&&_wcsicmp(cls,L"TNewRadioButton")&&
       _wcsicmp(cls,L"TNewCheckBox")&&_wcsicmp(cls,L"TNewButton"))return nullptr;
    COLORREF background=GetBkColor(dc);
    if(background!=Win32MainDark&&!InstallerNeutralSurface(background))return nullptr;
    return owner;
}
class InstallerNativeCaptionScope {
    HDC dc=nullptr;
    COLORREF previousText=CLR_INVALID,previousBackground=CLR_INVALID;
    bool previousEntry=false;
public:
    explicit InstallerNativeCaptionScope(HDC candidate){
        HWND owner=InstallerNativeCaptionOwner(candidate);if(!owner)return;
        previousText=GetTextColor(candidate);previousBackground=GetBkColor(candidate);
        if(previousText==CLR_INVALID||previousBackground==CLR_INVALID)return;
        dc=candidate;previousEntry=g_glowEntry;g_glowEntry=true;
        const int low=std::min({GetRValue(previousText),GetGValue(previousText),GetBValue(previousText)});
        const int high=std::max({GetRValue(previousText),GetGValue(previousText),GetBValue(previousText)});
        if(high-low<=12&&high<=64)
            SetTextColor(dc,IsWindowEnabled(owner)?WinUI::Dark::TextFillColorPrimary:RGB(157,157,157));
        SetBkColor_orig(dc,Win32MainDark);
    }
    ~InstallerNativeCaptionScope(){
        if(!dc)return;
        SetBkColor_orig(dc,previousBackground);SetTextColor(dc,previousText);
        g_glowEntry=previousEntry;
    }
    InstallerNativeCaptionScope(const InstallerNativeCaptionScope&)=delete;
    InstallerNativeCaptionScope& operator=(const InstallerNativeCaptionScope&)=delete;
    bool Active()const{return dc!=nullptr;}
    COLORREF Color()const{return dc?GetTextColor(dc):CLR_INVALID;}
    DTTOPTS ThemeOptions(const DTTOPTS* input)const{
        DTTOPTS value{sizeof(DTTOPTS)};if(input)value=*input;
        // Explicit authored colored captions keep their color. Neutral theme
        // black is replaced only on this known restyled caption surface.
        bool authored=false;
        if(input&&(input->dwFlags&DTT_TEXTCOLOR)){
            int low=std::min({GetRValue(input->crText),GetGValue(input->crText),GetBValue(input->crText)});
            int high=std::max({GetRValue(input->crText),GetGValue(input->crText),GetBValue(input->crText)});
            authored=high-low>12||high>64;
        }
        if(!authored){value.dwFlags|=DTT_TEXTCOLOR;value.crText=Color();}
        return value;
    }
};
