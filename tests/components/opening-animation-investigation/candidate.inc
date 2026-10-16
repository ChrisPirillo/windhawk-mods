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
