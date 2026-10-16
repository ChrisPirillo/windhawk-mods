// Regular/bold captions share a measured baseline.
static bool DrawNativePropertyTabCaption(HTHEME theme,HDC dc,int part,int state,
    LPCWSTR text,int count,DWORD flags,LPCRECT source,const DTTOPTS* options) {
    if(!g_settings.FlatPropertyPresentation || !theme || !dc || !source || !text || (flags&DT_CALCRECT) ||
        IsWinUiTabStyleActive() || !IsCustomDarkModeAllowed() || SysHighContrastEnabled() ||
        g_tabFeaturesUnloading.load(std::memory_order_acquire) || GetCachedThemeClass(theme)!=L"Tab" ||
        part<TABP_TABITEM || part>TABP_TOPTABITEMBOTHEDGE)return false;
    HWND tab=WindowFromDC(dc);if(!IsClassName(tab,L"SysTabControl32"))tab=g_tlsPaintHwnd;
    HWND root=tab?GetAncestor(tab,GA_ROOT):nullptr;
    if(!IsClassName(tab,L"SysTabControl32") || !IsClassName(root,L"#32770") ||
        !GetPropW(root,kPropertyDialogDarkModeMarker) ||
        FindWindowExW(root,nullptr,L"SysTabControl32",nullptr)!=tab ||
        (GetWindowLongPtrW(tab,GWL_STYLE)&(TCS_VERTICAL|TCS_BOTTOM|TCS_OWNERDRAWFIXED)))return false;
    TCHITTESTINFO hit{};hit.pt={(source->left+source->right)/2,(source->top+source->bottom)/2};
    const int index=TabCtrl_HitTest(tab,&hit);if(index<0)return false;
    TCITEMW item{};item.mask=TCIF_IMAGE;item.iImage=-1;
    if(TabCtrl_GetItem(tab,index,&item) && item.iImage>=0)return false;
    RECT rect{};if(!TabCtrl_GetItemRect(tab,index,&rect))return false;
    const int inset=0; // Native item padding already reserves caption breathing room.
    rect.left+=inset;rect.right-=inset;
    // Selected native items can be inflated vertically. Use the common row
    // interior so changing selection never moves a caption's baseline.
    if(TabCtrl_GetRowCount(tab)==1){
        for(int i=0;i<TabCtrl_GetItemCount(tab);i++){
            RECT peer{};if(TabCtrl_GetItemRect(tab,i,&peer)){
                rect.top=(std::max)(rect.top,peer.top);
                rect.bottom=(std::min)(rect.bottom,peer.bottom);
            }
        }
    }
    rect.top+=1;rect.bottom-=1;
    HFONT font=reinterpret_cast<HFONT>(GetCurrentObject(dc,OBJ_FONT));LOGFONTW description{};
    font=NativePropertyCaptionBaseFont(tab,font);
    if(!font || !GetObjectW(font,sizeof(description),&description))return false;
    description.lfWeight=(state==TIS_SELECTED || state==TIS_FOCUSED)?FW_BOLD:FW_NORMAL;
    HFONT caption=g_winUiTabFontCache.Ensure(description);if(!caption)return false;
    if(options && (options->dwFlags&(DTT_CALLBACK|DTT_CALCRECT)))return false;
    DTTOPTS draw{sizeof(DTTOPTS)};
    draw.dwFlags=DTT_TEXTCOLOR|(options?(options->dwFlags&DTT_COMPOSITED):0);
    draw.crText=state==TIS_DISABLED?RGB(128,128,128):RGB(235,235,235);
    LOGFONTW regularDescription=description;regularDescription.lfWeight=FW_NORMAL;
    HFONT regular=g_winUiTabFontCache.Ensure(regularDescription);
    LOGFONTW boldDescription=description;boldDescription.lfWeight=FW_BOLD;
    HFONT bold=g_winUiTabFontCache.Ensure(boldDescription);
    if(!regular || !bold)return false;
    HGDIOBJ previous=SelectObject(dc,regular);TEXTMETRICW regularMetric{},boldMetric{};
    GetTextMetricsW(dc,&regularMetric);SelectObject(dc,bold);GetTextMetricsW(dc,&boldMetric);
    const int ascent=(std::max)(regularMetric.tmAscent,boldMetric.tmAscent);
    const int descent=(std::max)(regularMetric.tmDescent,boldMetric.tmDescent);
    const int baseline=rect.top+(rect.bottom-rect.top-ascent-descent)/2+ascent;
    SelectObject(dc,caption);TEXTMETRICW metric{};GetTextMetricsW(dc,&metric);
    rect.top=baseline-metric.tmAscent;
    // Part zero explicitly uses the selected DC font; a themed part can
    // replace it with the theme's normal font and silently lose bold weight.
    const HRESULT result=DrawThemeTextEx_orig(theme,dc,0,0,text,count,
        (flags&~(DT_WORDBREAK|DT_VCENTER|DT_BOTTOM))|DT_SINGLELINE|DT_TOP|DT_CENTER,&rect,&draw);
    SelectObject(dc,previous);return SUCCEEDED(result);
}
