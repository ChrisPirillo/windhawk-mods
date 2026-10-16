static constexpr wchar_t kNativePropertyTabFont[] = L"W32M.PropertyTabFont";
static constexpr wchar_t kNativePropertyTabFontHook[] = L"W32M.PropertyTabFontHook";
struct NativePropertyTabFontState { HFONT original, assigned; };
static bool NativePropertyTabFontEligible(HWND tab){
    return FlatNativePropertyChrome(tab) && IsClassName(tab,L"SysTabControl32") &&
        !g_tabFeaturesUnloading.load(std::memory_order_acquire) &&
        !g_nativeUiFontUnloading.load(std::memory_order_acquire);
}
static void RestoreNativePropertyTabFont(HWND tab){
    auto* state=reinterpret_cast<NativePropertyTabFontState*>(RemovePropW(tab,kNativePropertyTabFont));
    if(!state)return;
    if(reinterpret_cast<HFONT>(SendMessageW(tab,WM_GETFONT,0,0))==state->assigned){
        const bool prior=t_nativePropertyFontUpdate;t_nativePropertyFontUpdate=true;
        SendMessageW(tab,WM_SETFONT,reinterpret_cast<WPARAM>(state->original),FALSE);
        t_nativePropertyFontUpdate=prior;
    }
    DeleteObject(state->assigned);delete state;
}
static LRESULT CALLBACK NativePropertyTabFontSubclass(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);
static void ApplyNativePropertyTabFont(HWND tab){
    if(!NativePropertyTabFontEligible(tab) || GetPropW(tab,kNativePropertyTabFont))return;
    HFONT original=reinterpret_cast<HFONT>(SendMessageW(tab,WM_GETFONT,0,0));
    LOGFONTW description{};if(!original || !GetObjectW(original,sizeof(description),&description))return;
    // Native font proportions must remain unchanged on every paint route.
    description.lfWeight=FW_BOLD; // Reserve natural width for the selected caption.
    HFONT assigned=CreateFontIndirectW(&description);if(!assigned)return;
    auto* state=new NativePropertyTabFontState{original,assigned};
    if(!SetPropW(tab,kNativePropertyTabFont,state)){DeleteObject(assigned);delete state;return;}
    if(!GetPropW(tab,kNativePropertyTabFontHook)){
        if(!WindhawkUtils::SetWindowSubclassFromAnyThread(tab,NativePropertyTabFontSubclass,0)){
            RemovePropW(tab,kNativePropertyTabFont);DeleteObject(assigned);delete state;return;
        }
        if(!SetPropW(tab,kNativePropertyTabFontHook,reinterpret_cast<HANDLE>(1))){
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(tab,NativePropertyTabFontSubclass);
            RemovePropW(tab,kNativePropertyTabFont);DeleteObject(assigned);delete state;return;
        }
    }
    const bool prior=t_nativePropertyFontUpdate;t_nativePropertyFontUpdate=true;
    SendMessageW(tab,WM_SETFONT,reinterpret_cast<WPARAM>(assigned),FALSE);
    t_nativePropertyFontUpdate=prior;
}
static void RemoveNativePropertyTabFont(HWND tab){
    RestoreNativePropertyTabFont(tab);
    if(RemovePropW(tab,kNativePropertyTabFontHook))
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(tab,NativePropertyTabFontSubclass);
}
static LRESULT CALLBACK NativePropertyTabFontSubclass(HWND tab,UINT message,
    WPARAM wp,LPARAM lp,DWORD_PTR){
    if(message==WM_NCDESTROY){
        auto* state=reinterpret_cast<NativePropertyTabFontState*>(RemovePropW(tab,kNativePropertyTabFont));
        RemovePropW(tab,kNativePropertyTabFontHook);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(tab,NativePropertyTabFontSubclass);
        LRESULT result=DefSubclassProc(tab,message,wp,lp);
        if(state){DeleteObject(state->assigned);delete state;}
        return result;
    }
    const bool assignment=message==WM_SETFONT && !t_nativePropertyFontUpdate;
    if(assignment)RestoreNativePropertyTabFont(tab);
    LRESULT result=DefSubclassProc(tab,message,wp,lp);
    if((assignment || message==g_nativeUiFontSyncMessage) && !t_nativePropertyFontUpdate)
        ApplyNativePropertyTabFont(tab);
    return result;
}
static HFONT NativePropertyCaptionBaseFont(HWND tab,HFONT fallback){
    auto* state=reinterpret_cast<NativePropertyTabFontState*>(GetPropW(tab,kNativePropertyTabFont));
    return state?state->original:fallback;
}

// The general font policy must see through our owned role font on repaint.
// Otherwise it can save that temporary HFONT as the application's original.
static HFONT NativePropertyTabUnderlyingFont(HWND tab,HFONT current){
    auto* state=reinterpret_cast<NativePropertyTabFontState*>(GetPropW(tab,kNativePropertyTabFont));
    return state && current==state->assigned ? state->original : current;
}
