static decltype(&ShowWindow) OpeningShowWindow_orig=nullptr;
static decltype(&ShowWindowAsync) OpeningShowWindowAsync_orig=nullptr;
static decltype(&SetWindowPos) OpeningSetWindowPos_orig=nullptr;
static decltype(&AnimateWindow) OpeningAnimateWindow_orig=nullptr;
static void OpeningPrepareNativeShow(HWND window,bool show,bool hide){
    if(!show && !hide)return;
    auto state=OpeningTransitionFind(window);if(!state)return;
    if(hide)OpeningTransitionRelease(state);
    else if(OpeningTransitionEnabled() && !IsWindowVisible(window))
        OpeningTransitionArm(state);
}
static bool OpeningShowCommand(int command){
    return command!=SW_HIDE && command!=SW_MINIMIZE &&
        command!=SW_SHOWMINIMIZED && command!=SW_SHOWMINNOACTIVE &&
        command!=SW_FORCEMINIMIZE;
}
static BOOL WINAPI OpeningShowWindow_hook(HWND window,int command){
    const DWORD incomingError=GetLastError();
    OpeningPrepareNativeShow(window,OpeningShowCommand(command),command==SW_HIDE);
    SetLastError(incomingError);
    return OpeningShowWindow_orig(window,command);
}
static BOOL WINAPI OpeningShowWindowAsync_hook(HWND window,int command){
    const DWORD incomingError=GetLastError();
    OpeningPrepareNativeShow(window,OpeningShowCommand(command),command==SW_HIDE);
    SetLastError(incomingError);
    return OpeningShowWindowAsync_orig(window,command);
}
static BOOL WINAPI OpeningSetWindowPos_hook(HWND window,HWND after,int x,int y,int cx,int cy,UINT flags){
    const DWORD incomingError=GetLastError();
    OpeningPrepareNativeShow(window,(flags&SWP_SHOWWINDOW)!=0,(flags&SWP_HIDEWINDOW)!=0);
    SetLastError(incomingError);
    return OpeningSetWindowPos_orig(window,after,x,y,cx,cy,flags);
}
static BOOL WINAPI OpeningAnimateWindow_hook(HWND window,DWORD duration,DWORD flags){
    const DWORD incomingError=GetLastError();
    if(flags&AW_HIDE)OpeningPrepareNativeShow(window,false,true);
    if(!(flags&AW_HIDE) && OpeningTransitionEnabled() &&
        !IsWindowVisible(window) && OpeningTransitionFind(window)){
        OpeningPrepareNativeShow(window,true,false);
        ShowWindow(window,(flags&AW_ACTIVATE)?SW_SHOW:SW_SHOWNOACTIVATE);
        return IsWindowVisible(window);
    }
    SetLastError(incomingError);
    return OpeningAnimateWindow_orig(window,duration,flags);
}

