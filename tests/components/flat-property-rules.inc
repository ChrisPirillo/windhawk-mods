static constexpr wchar_t kFlatPropertyRule[]=L"W32M.FlatPropertyRule";
static bool FlatPropertyRuleEligible(HWND w){
    if(!FlatNativePropertyChrome(w)||!IsClassName(w,L"Static")||GetWindowTextLengthW(w)!=0)return false;
    const LONG type=GetWindowLongPtrW(w,GWL_STYLE)&SS_TYPEMASK;
    if(type==SS_ICON||type==SS_BITMAP||type==SS_ENHMETAFILE||type==SS_OWNERDRAW)return false;
    RECT r{};GetWindowRect(w,&r);const int width=r.right-r.left,height=r.bottom-r.top;
    return height>0&&height<=MulDiv(6,GetDpiForWindow(w),96)&&width>4*height;
}
static LRESULT CALLBACK FlatPropertyRuleSubclass(HWND w,UINT m,WPARAM a,LPARAM b,DWORD_PTR){
    if(m==WM_NCDESTROY){RemovePropW(w,kFlatPropertyRule);WindhawkUtils::RemoveWindowSubclassFromAnyThread(w,FlatPropertyRuleSubclass);return DefSubclassProc(w,m,a,b);}
    const bool painting=m==WM_NCPAINT||m==WM_PRINT||m==WM_PAINT||m==WM_PRINTCLIENT||m==WM_ERASEBKGND;
    if(painting&&GetPropW(w,kFlatPropertyRule)&&FlatPropertyRuleEligible(w)&&!g_tabFeaturesUnloading.load(std::memory_order_acquire)){
        if(m==WM_NCPAINT){const LRESULT result=DefSubclassProc(w,m,a,b);HDC dc=GetWindowDC(w);if(dc){RECT r{};GetWindowRect(w,&r);OffsetRect(&r,-r.left,-r.top);FillRect_orig(dc,&r,GetCachedSolidBrush(Win32MainDark));ReleaseDC(w,dc);}return result;}
        if(m==WM_PRINT&&a&&(b&PRF_NONCLIENT)){RECT r{};GetWindowRect(w,&r);OffsetRect(&r,-r.left,-r.top);FillRect_orig((HDC)a,&r,GetCachedSolidBrush(Win32MainDark));return 0;}
        if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);FillRect_orig(dc,&r,GetCachedSolidBrush(Win32MainDark));EndPaint(w,&ps);return 0;}
        if((m==WM_PRINTCLIENT||m==WM_ERASEBKGND)&&a){RECT r{};GetClientRect(w,&r);FillRect_orig((HDC)a,&r,GetCachedSolidBrush(Win32MainDark));return m==WM_ERASEBKGND?1:0;}
    }
    return DefSubclassProc(w,m,a,b);
}
static BOOL CALLBACK InstallFlatPropertyRuleChild(HWND w,LPARAM){
    if(FlatPropertyRuleEligible(w)&&!GetPropW(w,kFlatPropertyRule)&&!g_tabFeaturesUnloading.load(std::memory_order_acquire)){
        if(WindhawkUtils::SetWindowSubclassFromAnyThread(w,FlatPropertyRuleSubclass,0)){
            if(!SetPropW(w,kFlatPropertyRule,(HANDLE)1))WindhawkUtils::RemoveWindowSubclassFromAnyThread(w,FlatPropertyRuleSubclass);
            InvalidateRect(w,nullptr,TRUE);
        }
    }return TRUE;
}
static BOOL CALLBACK RemoveFlatPropertyRuleChild(HWND w,LPARAM){
    if(RemovePropW(w,kFlatPropertyRule)){WindhawkUtils::RemoveWindowSubclassFromAnyThread(w,FlatPropertyRuleSubclass);InvalidateRect(w,nullptr,TRUE);}return TRUE;
}
static void RemoveFlatPropertyRules(HWND root){EnumChildWindows(root,RemoveFlatPropertyRuleChild,0);}
