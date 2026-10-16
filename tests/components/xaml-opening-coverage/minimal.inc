using NtUserCreateWindowEx_t = HWND(WINAPI*)(DWORD, VOID*, LPCWSTR, VOID*, DWORD, LONG, LONG,
    LONG, LONG, HWND, HMENU, HINSTANCE, LPVOID, DWORD, DWORD, DWORD, VOID*);
NtUserCreateWindowEx_t NtUserCreateWindowEx_orig = nullptr;

// XAML hosts keep the menu-only fast path. Observe native creation without
// entering the normal font, widget, dialog-staging or theme initialization.
static HWND WINAPI OpeningNtUserCreateWindowEx_hook(DWORD dwExStyle, VOID* pClassName, LPCWSTR lpWindowName,
    VOID* pTitleName, DWORD dwStyle, LONG X, LONG Y, LONG nWidth, LONG nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam,
    DWORD dwShowCmd, DWORD dwFlags1, DWORD dwFlags2, VOID* qwFlags)
{
    const DWORD incomingError=GetLastError();
    static thread_local bool installingOpeningHook=false;
    if((g_settings.DisableWindowOpeningAnimations || ConsistentCaptionTrackingEnabled()) && !installingOpeningHook){
        installingOpeningHook=true;
        InstallCbtHookForThread();
        installingOpeningHook=false;
    }
    SetLastError(incomingError);
    return NtUserCreateWindowEx_orig(dwExStyle,pClassName,lpWindowName,pTitleName,
        dwStyle,X,Y,nWidth,nHeight,hWndParent,hMenu,hInstance,lpParam,
        dwShowCmd,dwFlags1,dwFlags2,qwFlags);
}


