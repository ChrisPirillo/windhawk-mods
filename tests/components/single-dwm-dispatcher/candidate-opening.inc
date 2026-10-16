struct OpeningTransitionState {
    HWND window;
    std::recursive_mutex assignmentMutex;
    std::atomic<bool> requested{false}, owned{false}, painted{false}, hidden{true};
    std::atomic<ULONGLONG> started{0};
    UINT_PTR timer()const{return reinterpret_cast<UINT_PTR>(this);}
};
static std::mutex g_openingTransitionsMutex;
static std::unordered_map<HWND,std::shared_ptr<OpeningTransitionState>> g_openingTransitions;
static std::atomic<bool> g_openingTransitionsUnloading{false};
static std::shared_ptr<OpeningTransitionState> OpeningTransitionFind(HWND window){
    std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);
    auto it=g_openingTransitions.find(window);
    return it==g_openingTransitions.end()?nullptr:it->second;
}
static void OpeningTransitionRelease(const std::shared_ptr<OpeningTransitionState>& state){
    std::lock_guard<std::recursive_mutex> lock(state->assignmentMutex);
    KillTimer(state->window,state->timer());
    if(state->owned.exchange(false) && OpeningDwmSetAttribute_orig){
        BOOL requested=state->requested.load();
        OpeningDwmSetAttribute_orig(state->window,DWMWA_TRANSITIONS_FORCEDISABLED,
            &requested,sizeof(requested));
        const BOOL latest=state->requested.load();
        if(latest!=requested)OpeningDwmSetAttribute_orig(state->window,
            DWMWA_TRANSITIONS_FORCEDISABLED,&latest,sizeof(latest));
    }
}
static bool OpeningTransitionEnabled(){
    return g_settings.DisableWindowOpeningAnimations &&
        !g_openingTransitionsUnloading.load(std::memory_order_acquire);
}
static void OpeningTransitionArm(const std::shared_ptr<OpeningTransitionState>& state){
    std::lock_guard<std::recursive_mutex> lock(state->assignmentMutex);
    if(!OpeningTransitionEnabled() || state->owned || !OpeningDwmSetAttribute_orig)return;
    BOOL disabled=TRUE;
    if(FAILED(OpeningDwmSetAttribute_orig(state->window,DWMWA_TRANSITIONS_FORCEDISABLED,
            &disabled,sizeof(disabled))))return;
    state->owned=true;state->painted=false;state->started=0;
    if(!OpeningTransitionEnabled())OpeningTransitionRelease(state);
}
static void OpeningTransitionStart(const std::shared_ptr<OpeningTransitionState>& state){
    std::lock_guard<std::recursive_mutex> lock(state->assignmentMutex);
    OpeningTransitionArm(state);
    if(!state->owned)return;
    state->painted=false;state->started=GetTickCount64();
    if(!SetTimer(state->window,state->timer(),16,nullptr) || !OpeningTransitionEnabled())
        OpeningTransitionRelease(state);
}
static HRESULT WINAPI OpeningDwmSetAttribute_hook(HWND window,DWORD attribute,
    LPCVOID value,DWORD size){
    if(attribute==35 || attribute==36)return ConsistentCaptionDwmSet(window,attribute,value,size);
    if(attribute!=DWMWA_TRANSITIONS_FORCEDISABLED || !value || size!=sizeof(BOOL))
        return OpeningDwmSetAttribute_orig(window,attribute,value,size);
    auto state=OpeningTransitionFind(window);
    if(!state)return OpeningDwmSetAttribute_orig(window,attribute,value,size);
    std::lock_guard<std::recursive_mutex> lock(state->assignmentMutex);
    const BOOL requested=*static_cast<const BOOL*>(value);
    const bool previous=state->requested.exchange(requested!=FALSE);
    BOOL applied=state->owned && OpeningTransitionEnabled()?TRUE:requested;
    HRESULT result=OpeningDwmSetAttribute_orig(window,attribute,&applied,sizeof(applied));
    if(FAILED(result)){
        bool expected=requested!=FALSE;
        state->requested.compare_exchange_strong(expected,previous);
    }else{
        const BOOL desired=state->owned && OpeningTransitionEnabled()
            ? TRUE:static_cast<BOOL>(state->requested.load());
        if(desired!=applied)OpeningDwmSetAttribute_orig(window,attribute,&desired,sizeof(desired));
    }
    return result;
}
static LRESULT CALLBACK OpeningTransitionSubclass(HWND window,UINT message,
    WPARAM wp,LPARAM lp,DWORD_PTR){
    auto state=OpeningTransitionFind(window);
    if(!state)return DefSubclassProc(window,message,wp,lp);
    if(message==WM_CLOSE || message==WM_DESTROY ||
        (message==WM_SYSCOMMAND && (wp&0xFFF0)==SC_CLOSE))
        OpeningTransitionRelease(state); // Restore BEFORE native closing logic.
    if(message==WM_SHOWWINDOW){
        if(wp){if(state->hidden.exchange(false))OpeningTransitionStart(state);}
        else{state->hidden=true;OpeningTransitionRelease(state);}
    }
    if(message==WM_WINDOWPOSCHANGING && lp){
        auto position=reinterpret_cast<const WINDOWPOS*>(lp);
        if((position->flags&SWP_SHOWWINDOW) && state->hidden.exchange(false))
            OpeningTransitionStart(state);
        if(position->flags&SWP_HIDEWINDOW){state->hidden=true;OpeningTransitionRelease(state);}
    }
    if(message==WM_TIMER && wp==state->timer()){
        if(DialogPresentationHoldTransitions(window))return 0;
        const ULONGLONG elapsed=GetTickCount64()-state->started.load();
        if(!OpeningTransitionEnabled() || (state->painted && elapsed>=64) || elapsed>=500)
            OpeningTransitionRelease(state);
        return 0;
    }
    // HCBT_CREATEWND precedes a usable DWM handle on some Windows builds.
    // Retry as soon as native creation begins, before application handlers can
    // request their first show; retain the post-handler retry for transient failure.
    if(message==WM_NCCREATE || message==WM_CREATE)OpeningTransitionArm(state);
    LRESULT result=DefSubclassProc(window,message,wp,lp);
    if(message==WM_NCCREATE || message==WM_CREATE)OpeningTransitionArm(state);
    if(message==WM_PAINT && state->owned)state->painted=true;
    if(message==WM_NCDESTROY){
        OpeningTransitionRelease(state);
        {std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);g_openingTransitions.erase(window);}
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window,OpeningTransitionSubclass);
    }
    return result;
}
static void OpeningTransitionObserveNewWindow(HWND window,const CREATESTRUCTW* creation){
    if(!OpeningTransitionEnabled() || !creation || (creation->style&WS_CHILD) ||
        creation->hwndParent==HWND_MESSAGE)return;
    const bool dialog=IsClassName(window,L"#32770");
    const bool regedit=IsCurrentProcessRegedit() && IsClassName(window,L"RegEdit_RegEdit");
    const bool installer=g_settings.InstallerUi && (IsClassName(window,L"TWizardForm") ||
        IsClassName(window,L"TUninstallProgressForm") || IsClassName(window,L"WixStdBA"));
    // Broader coverage is opt-in and only owns ordinary application frames.
    // Auxiliary popups/control animations remain native unless explicitly enrolled.
    const bool applicationFrame=g_settings.OpeningAnimationAllApplications &&
        (((creation->style&WS_CAPTION)==WS_CAPTION) || (creation->style&WS_THICKFRAME)) &&
        !(creation->dwExStyle&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE));
    if(!dialog && !regedit && !installer && !g_settings.OpeningAnimationProcessTarget &&
       !applicationFrame)return;
    auto state=std::make_shared<OpeningTransitionState>();state->window=window;
    {std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);
        if(g_openingTransitions.size()>=256 || g_openingTransitions.count(window))return;
        g_openingTransitions.emplace(window,state);}
    if(!WindhawkUtils::SetWindowSubclassFromAnyThread(window,OpeningTransitionSubclass,0)){
        std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);g_openingTransitions.erase(window);
    }else if(g_openingTransitionsUnloading.load(std::memory_order_acquire)){
        OpeningTransitionRelease(state);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window,OpeningTransitionSubclass);
        std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);g_openingTransitions.erase(window);
    }else{
        // Pre-arm before native creation; start the deadline only on show.
        OpeningTransitionArm(state);
    }
}
static void OpeningTransitionsRestoreAll(bool unloading){
    if(unloading)g_openingTransitionsUnloading.store(true,std::memory_order_release);
    std::vector<std::shared_ptr<OpeningTransitionState>> states;
    {std::lock_guard<std::mutex> lock(g_openingTransitionsMutex);
        for(const auto& [window,state]:g_openingTransitions)states.push_back(state);
        if(unloading)g_openingTransitions.clear();}
    for(const auto& state:states){
        OpeningTransitionRelease(state);
        if(unloading)WindhawkUtils::RemoveWindowSubclassFromAnyThread(
            state->window,OpeningTransitionSubclass);
    }
}




