// CAPTION_COLOR/TEXT_COLOR are setter attributes on current Windows builds.
// An observed new lifetime starts at documented COLOR_DEFAULT. Existing
// unknown windows are never assigned a guessed restoration baseline.
struct ConsistentCaptionSlot {
    bool known=false,owned=false,superseded=false;
    const void* caller=nullptr;
    COLORREF requested=0xFFFFFFFF,assigned=0xFFFFFFFF;
    unsigned long long revision=0;
};
struct ConsistentCaptionState {
    HWND window=nullptr;
    std::recursive_mutex assignmentMutex;
    bool retired=false;
    ConsistentCaptionSlot colors[2];
};
static std::mutex g_consistentCaptionsMutex;
static std::unordered_map<HWND,std::shared_ptr<ConsistentCaptionState>>g_consistentCaptions;
static std::atomic<bool>g_consistentCaptionsUnloading{false};
static constexpr wchar_t kConsistentCaptionOwner[]=L"W32M.CaptionColorOwner";
static LRESULT CALLBACK ConsistentCaptionSubclass(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);
static std::shared_ptr<ConsistentCaptionState> ConsistentCaptionFind(HWND window){
    std::lock_guard<std::mutex>lock(g_consistentCaptionsMutex);
    auto i=g_consistentCaptions.find(window);
    return i==g_consistentCaptions.end()?nullptr:i->second;
}
static std::shared_ptr<ConsistentCaptionState> ConsistentCaptionEnsure(HWND window,bool fresh){
    if(auto existing=ConsistentCaptionFind(window))return existing;
    if(g_consistentCaptionsUnloading.load(std::memory_order_acquire))return nullptr;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId()||GetAncestor(window,GA_ROOT)!=window)return nullptr;
    auto state=std::make_shared<ConsistentCaptionState>();state->window=window;
    state->colors[0].known=state->colors[1].known=fresh;
    {
        std::lock_guard<std::mutex>lock(g_consistentCaptionsMutex);
        if(g_consistentCaptions.size()>=512)return nullptr;
        auto result=g_consistentCaptions.emplace(window,state);
        if(!result.second)return result.first->second;
    }
    if(!SetPropW(window,kConsistentCaptionOwner,state.get())||
       !WindhawkUtils::SetWindowSubclassFromAnyThread(window,ConsistentCaptionSubclass,0)){
        if(GetPropW(window,kConsistentCaptionOwner)==state.get())RemovePropW(window,kConsistentCaptionOwner);
        std::lock_guard<std::mutex>lock(g_consistentCaptionsMutex);
        auto i=g_consistentCaptions.find(window);if(i!=g_consistentCaptions.end()&&i->second==state)g_consistentCaptions.erase(i);
        return nullptr;
    }
    return state;
}
static void ConsistentCaptionObserveNewWindow(HWND window,const CREATESTRUCTW* creation){
    if(!creation||(creation->style&WS_CHILD)||creation->hwndParent==HWND_MESSAGE)return;
    ConsistentCaptionEnsure(window,true);
}
static bool ConsistentCaptionStillOwned(const std::shared_ptr<ConsistentCaptionState>&state){
    return !state->retired&&GetPropW(state->window,kConsistentCaptionOwner)==state.get();
}
static HRESULT ConsistentCaptionDwmSet(HWND window,DWORD attribute,LPCVOID value,DWORD size){
    if((attribute!=35&&attribute!=36)||!value||size!=sizeof(COLORREF))
        return OpeningDwmSetAttribute_orig(window,attribute,value,size);
    auto state=ConsistentCaptionEnsure(window,false);
    if(!state)return OpeningDwmSetAttribute_orig(window,attribute,value,size);
    std::lock_guard<std::recursive_mutex>lock(state->assignmentMutex);
    auto&slot=state->colors[attribute-35];const auto revision=slot.revision;
    const HRESULT result=OpeningDwmSetAttribute_orig(window,attribute,value,size);
    if(SUCCEEDED(result)&&ConsistentCaptionStillOwned(state)){
        if(slot.revision!=revision){
            // Nested successful writes have ambiguous native completion order.
            // A later unambiguous application write can establish a new baseline.
            slot.known=false;slot.owned=false;slot.superseded=true;++slot.revision;return result;
        }
        slot.requested=*static_cast<const COLORREF*>(value);slot.known=true;
        // An application write is authoritative. Do not undo it on cleanup.
        if(slot.owned)slot.superseded=true;
        slot.owned=false;++slot.revision;
    }
    return result;
}
static constexpr unsigned char kConsistentCaptionGenericCaller=0;
// A null caller is reserved for final global restoration. Every normal
// assignment/release has an explicit internal owner, independently per color.
static void ConsistentCaptionAttributeForOwner(HWND window,DWORD attribute,bool apply,COLORREF color,const void* caller){
    if(attribute!=35&&attribute!=36)return;
    auto state=ConsistentCaptionFind(window);if(!state||!OpeningDwmSetAttribute_orig)return;
    std::lock_guard<std::recursive_mutex>lock(state->assignmentMutex);
    if(!ConsistentCaptionStillOwned(state))return;
    auto&slot=state->colors[attribute-35];
    if(!caller&&apply)return;
    if(caller&&slot.caller&&slot.caller!=caller)return;
    if(!apply){
        if(slot.owned&&slot.known){
            const auto revision=slot.revision;
            const HRESULT result=OpeningDwmSetAttribute_orig(window,attribute,&slot.requested,sizeof(slot.requested));
            if(slot.revision!=revision){slot.known=false;slot.owned=false;slot.superseded=true;slot.caller=caller;return;}
            if(SUCCEEDED(result))slot.owned=false;
        }
        if(!slot.owned){slot.superseded=false;slot.caller=nullptr;} // Only this owner's off cycle can reset it.
        return;
    }
    if(!slot.known||slot.superseded||g_consistentCaptionsUnloading.load(std::memory_order_acquire))return;
    if(slot.owned&&slot.assigned==color)return;
    const auto revision=slot.revision;
    const HRESULT result=OpeningDwmSetAttribute_orig(window,attribute,&color,sizeof(color));
    if(slot.revision!=revision){slot.known=false;slot.owned=false;slot.superseded=true;slot.caller=caller;return;}
    if(SUCCEEDED(result)&&ConsistentCaptionStillOwned(state)){slot.assigned=color;slot.owned=true;slot.caller=caller;}
}
static void ConsistentCaptionAttribute(HWND window,DWORD attribute,LPCWSTR,LPCWSTR,LPCWSTR,bool apply,COLORREF color){
    ConsistentCaptionAttributeForOwner(window,attribute,apply,color,&kConsistentCaptionGenericCaller);
}
static LRESULT CALLBACK ConsistentCaptionSubclass(HWND window,UINT message,WPARAM wp,LPARAM lp,DWORD_PTR){
    if(message==WM_NCDESTROY){
        if(auto state=ConsistentCaptionFind(window)){
            std::lock_guard<std::recursive_mutex>lock(state->assignmentMutex);state->retired=true;
            if(GetPropW(window,kConsistentCaptionOwner)==state.get())RemovePropW(window,kConsistentCaptionOwner);
            std::lock_guard<std::mutex>mapLock(g_consistentCaptionsMutex);
            auto i=g_consistentCaptions.find(window);if(i!=g_consistentCaptions.end()&&i->second==state)g_consistentCaptions.erase(i);
        }
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window,ConsistentCaptionSubclass);
    }
    return DefSubclassProc(window,message,wp,lp);
}
static void ConsistentCaptionsRestoreAll(){
    g_consistentCaptionsUnloading.store(true,std::memory_order_release);
    std::vector<std::shared_ptr<ConsistentCaptionState>>states;
    {std::lock_guard<std::mutex>lock(g_consistentCaptionsMutex);for(auto&entry:g_consistentCaptions)states.push_back(entry.second);}
    for(auto&state:states){
        ConsistentCaptionAttributeForOwner(state->window,35,false,0,nullptr);
        ConsistentCaptionAttributeForOwner(state->window,36,false,0,nullptr);
        {
            std::lock_guard<std::recursive_mutex>lock(state->assignmentMutex);
            if(GetPropW(state->window,kConsistentCaptionOwner)==state.get())RemovePropW(state->window,kConsistentCaptionOwner);
            state->retired=true;
        }
        // This helper synchronously messages another UI thread. Do not hold
        // either lock while waiting for an in-flight NCDESTROY callback.
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(state->window,ConsistentCaptionSubclass);
    }
    {std::lock_guard<std::mutex>lock(g_consistentCaptionsMutex);g_consistentCaptions.clear();}
}

