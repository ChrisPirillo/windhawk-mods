struct PaintOwnerFrame {
    HWND window, previous;
    bool insideExplorer, trackingExplorer;
};
static thread_local std::vector<PaintOwnerFrame> t_paintOwnerFrames;
static void PaintOwnerEnter(HWND window){
    t_paintOwnerFrames.push_back({window,g_tlsPaintHwnd,
        g_tlsPaintWasInsideExplorer,g_tlsExplorerMicaPaintTrackingEnabled});
    g_tlsPaintHwnd=window;
}
static void PaintOwnerLeave(HWND window,bool completed){
    if(t_paintOwnerFrames.empty() || t_paintOwnerFrames.back().window!=window)return;
    const auto frame=t_paintOwnerFrames.back();t_paintOwnerFrames.pop_back();
    g_tlsPaintHwnd=t_paintOwnerFrames.empty()
        ? (completed?window:frame.previous) : t_paintOwnerFrames.back().window;
    if(!completed || !t_paintOwnerFrames.empty()){
        g_tlsPaintWasInsideExplorer=frame.insideExplorer;
        g_tlsExplorerMicaPaintTrackingEnabled=frame.trackingExplorer;
    }
}

