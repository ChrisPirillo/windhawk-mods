// Translate a property's content as one group; never reflow labels or fields.
// Application layout changes supersede our saved positions, and unload restores
// only positions which still match the last position assigned here.
static constexpr wchar_t kBalancedPageState[] = L"W32M.BalancedPage";
static constexpr wchar_t kBalancedChildOwner[] = L"W32M.BalancedChild";
static constexpr wchar_t kNativeTabSavedSize[] = L"W32M.NativeTabSize";
struct BalancedChildPosition { POINT original{}, assigned{}; };
struct BalancedPageState {
    bool applying = false;
    bool resized = false;
    RECT originalPage{}, assignedPage{};
    std::unordered_map<HWND, BalancedChildPosition> children;
};
static UINT BalancedPageRestoreMessage() {
    static const UINT message=RegisterWindowMessageW(L"Win32UIModernizer.RestoreBalancedPage");
    return message;
}
static bool NativePropertyPresentationEnabled(HWND page) {
    return g_settings.FlatPropertyPresentation && IsCustomDarkModeAllowed() && !IsWinUiTabStyleActive() &&
        !SysHighContrastEnabled() && !g_tabFeaturesUnloading.load(std::memory_order_acquire) &&
        IsClassName(page, L"#32770") && HasTabControlSibling(page) &&
        (GetWindowLongPtrW(page, GWL_STYLE) & WS_CHILD);
}
static POINT BalancedChildPoint(HWND child, HWND page) {
    RECT rect{};GetWindowRect(child,&rect);
    MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&rect),2);
    return {rect.left,rect.top};
}
// Details owns a report list rather than a compact collection of fields.
// Measure its native rows/columns, preserving native hit testing and selection.
static constexpr wchar_t kNativeDetailsLayout[]=L"W32M.DetailsLayout";
struct NativeDetailsLayoutState {
    HWND list{},footer{};
    RECT originalList{},originalFooter{},assignedList{},assignedFooter{};
    int originalColumns[2]{},assignedColumns[2]{};
};
static RECT NativeDetailsRect(HWND window,HWND page){
    RECT r{};GetWindowRect(window,&r);MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&r),2);return r;
}
static void RestoreNativeDetailsLayout(HWND page){
    auto* s=reinterpret_cast<NativeDetailsLayoutState*>(RemovePropW(page,kNativeDetailsLayout));if(!s)return;
    if(IsWindow(s->list)&&GetParent(s->list)==page){
        for(int c=0;c<2;c++)if(ListView_GetColumnWidth(s->list,c)==s->assignedColumns[c])
            ListView_SetColumnWidth(s->list,c,s->originalColumns[c]);
        RECT now=NativeDetailsRect(s->list,page);
        if(EqualRect(&now,&s->assignedList))SetWindowPos(s->list,nullptr,s->originalList.left,s->originalList.top,
            s->originalList.right-s->originalList.left,s->originalList.bottom-s->originalList.top,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(IsWindow(s->footer)&&GetParent(s->footer)==page){
        RECT now=NativeDetailsRect(s->footer,page);
        if(EqualRect(&now,&s->assignedFooter))SetWindowPos(s->footer,nullptr,s->originalFooter.left,s->originalFooter.top,
            s->originalFooter.right-s->originalFooter.left,s->originalFooter.bottom-s->originalFooter.top,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    delete s;
}
static bool BalanceNativeDetailsPage(HWND page){
    HWND list=GetDlgItem(page,271),footer=GetDlgItem(page,272);
    if(!IsClassName(list,L"SysListView32")||!footer || GetParent(list)!=page || GetParent(footer)!=page)return false;
    HWND header=ListView_GetHeader(list);
    if(!header || Header_GetItemCount(header)!=2 || (GetWindowLongW(list,GWL_STYLE)&LVS_TYPEMASK)!=LVS_REPORT)return false;
    if(GetPropW(page,kNativeDetailsLayout))return true;
    const int rows=ListView_GetItemCount(list);
    if(rows<1 || rows>256 || ListView_GetImageList(list,LVSIL_SMALL) || ListView_GetTopIndex(list)!=0)return true;
    const UINT dpi=GetDpiForWindow(page);
    const int padding=MulDiv(20,dpi,96),margin=MulDiv(12,dpi,96),gap=MulDiv(12,dpi,96);
    RECT area{},client{};GetClientRect(page,&area);GetClientRect(list,&client);
    RECT listRect=NativeDetailsRect(list,page),footerRect=NativeDetailsRect(footer,page);
    int columns[2]{};HDC dc=GetDC(list);if(!dc)return true;
    HFONT font=reinterpret_cast<HFONT>(SendMessageW(list,WM_GETFONT,0,0));
    HGDIOBJ old=font?SelectObject(dc,font):nullptr;bool valid=true;int bottom=0;
    for(int c=0;c<2 && valid;c++){
        wchar_t text[2048]{};HDITEMW h{};h.mask=HDI_TEXT;h.pszText=text;h.cchTextMax=2048;
        if(!Header_GetItem(header,c,&h)){valid=false;break;}
        SIZE extent{};GetTextExtentPoint32W(dc,text,lstrlenW(text),&extent);columns[c]=extent.cx+padding;
        for(int row=0;row<rows;row++){
            LVITEMW value{};value.iSubItem=c;value.pszText=text;value.cchTextMax=2048;
            const int count=static_cast<int>(SendMessageW(list,LVM_GETITEMTEXT,row,reinterpret_cast<LPARAM>(&value)));
            if(count>=2047){valid=false;break;}
            if(!GetTextExtentPoint32W(dc,text,count,&extent)){valid=false;break;}
            columns[c]=(std::max)(columns[c],static_cast<int>(extent.cx)+padding);
        }
    }
    if(old)SelectObject(dc,old);ReleaseDC(list,dc);
    for(int row=0;row<rows && valid;row++){
        RECT r{};if(!ListView_GetItemRect(list,row,&r,LVIR_BOUNDS)||r.bottom<=r.top){valid=false;break;}
        bottom=(std::max)(bottom,static_cast<int>(r.bottom));
    }
    const int groups=static_cast<int>(SendMessageW(list,LVM_GETGROUPCOUNT,0,0));
    for(int i=0;i<groups && i<256 && valid;i++){
        LVGROUP group{};group.cbSize=sizeof(group);group.mask=LVGF_GROUPID;
        if(!SendMessageW(list,LVM_GETGROUPINFOBYINDEX,i,reinterpret_cast<LPARAM>(&group))){valid=false;break;}
        RECT r{};r.top=LVGGR_GROUP;
        if(!SendMessageW(list,LVM_GETGROUPRECT,group.iGroupId,reinterpret_cast<LPARAM>(&r))){valid=false;break;}
        bottom=(std::max)(bottom,static_cast<int>(r.bottom));
    }
    if(!valid || bottom<=0)return true;
    int width=columns[0]+columns[1]+GetSystemMetricsForDpi(SM_CXVSCROLL,dpi)+MulDiv(4,dpi,96)+(listRect.right-listRect.left-client.right);
    const int height=bottom+GetSystemMetricsForDpi(SM_CYHSCROLL,dpi)+MulDiv(6,dpi,96)+(listRect.bottom-listRect.top-client.bottom);
    const int footerWidth=footerRect.right-footerRect.left,footerHeight=footerRect.bottom-footerRect.top;
    const int totalHeight=height+gap+footerHeight;
    // A long value can need the native column widths while the short list
    // still benefits from vertical centering. Treat the two axes separately.
    const bool compactColumns=width<=area.right-2*margin;
    if(!compactColumns){width=listRect.right-listRect.left;for(int c=0;c<2;c++)columns[c]=ListView_GetColumnWidth(list,c);}
    if(footerWidth>area.right-2*margin ||
        totalHeight>area.bottom-2*margin || width<1 || height<1)return true;
    auto* state=new NativeDetailsLayoutState;state->list=list;state->footer=footer;
    state->originalList=listRect;state->originalFooter=footerRect;
    for(int c=0;c<2;c++)state->originalColumns[c]=ListView_GetColumnWidth(list,c);
    if(!SetPropW(page,kNativeDetailsLayout,state)){delete state;return true;}
    const int top=(area.bottom-totalHeight)/2;
    for(int c=0;c<2;c++)ListView_SetColumnWidth(list,c,columns[c]);
    const bool moved=SetWindowPos(list,nullptr,(area.right-width)/2,top,width,height,SWP_NOZORDER|SWP_NOACTIVATE) &&
        SetWindowPos(footer,nullptr,(area.right-footerWidth)/2,top+height+gap,footerWidth,footerHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    state->assignedList=NativeDetailsRect(list,page);state->assignedFooter=NativeDetailsRect(footer,page);
    for(int c=0;c<2;c++)state->assignedColumns[c]=ListView_GetColumnWidth(list,c);
    RECT after{};GetClientRect(list,&after);
    bool fits=moved && (compactColumns ? after.right>=columns[0]+columns[1] :
        state->assignedColumns[0]==state->originalColumns[0] && state->assignedColumns[1]==state->originalColumns[1]);
    for(int row=0;row<rows && fits;row++){RECT r{};fits=ListView_GetItemRect(list,row,&r,LVIR_BOUNDS)&&r.bottom<=after.bottom;}
    if(!fits)RestoreNativeDetailsLayout(page);
    return true;
}

static void RestoreBalancedPage(HWND page) {
    if (GetWindowThreadProcessId(page,nullptr)!=GetCurrentThreadId()) {
        DWORD_PTR ignored=0;
        SendMessageTimeoutW(page,BalancedPageRestoreMessage(),0,0,
            SMTO_ABORTIFHUNG|SMTO_BLOCK,500,&ignored);
        return;
    }
    RemovePropW(page,kNativePropertyPrepared);
    RestoreFolderLocationCaption(page);
    RestoreNativeDetailsLayout(page);
    auto* state = reinterpret_cast<BalancedPageState*>(RemovePropW(page,kBalancedPageState));
    if (!state) return;
    state->applying=true;
    for (const auto& [child,position]:state->children) {
        if (!IsWindow(child) || GetParent(child)!=page ||
            GetPropW(child,kBalancedChildOwner)!=state) continue;
        RemovePropW(child,kBalancedChildOwner);
        const POINT current=BalancedChildPoint(child,page);
        if (current.x==position.assigned.x && current.y==position.assigned.y)
            SetWindowPos(child,nullptr,position.original.x,position.original.y,0,0,
                SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS);
    }
    RECT pageRect{};GetWindowRect(page,&pageRect);
    MapWindowPoints(nullptr,GetParent(page),reinterpret_cast<POINT*>(&pageRect),2);
    if (state->resized && EqualRect(&pageRect,&state->assignedPage))
        SetWindowPos(page,nullptr,state->originalPage.left,state->originalPage.top,
            state->originalPage.right-state->originalPage.left,
            state->originalPage.bottom-state->originalPage.top,SWP_NOZORDER|SWP_NOACTIVATE);
    delete state;
}
static void RestoreNativePropertyTabSize(HWND root) {
    HWND tab=FindWindowExW(root,nullptr,L"SysTabControl32",nullptr);
    if(tab && GetWindowThreadProcessId(tab,nullptr)!=GetCurrentThreadId()){
        DWORD_PTR ignored=0;
        SendMessageTimeoutW(root,BalancedPageRestoreMessage(),0,0,
            SMTO_ABORTIFHUNG|SMTO_BLOCK,500,&ignored);
        return;
    }
    if(tab)RemoveNativePropertyTabFont(tab);
    const auto saved=reinterpret_cast<UINT_PTR>(RemovePropW(tab,kNativeTabSavedSize));
    if(saved)SendMessageW(tab,TCM_SETITEMSIZE,0,static_cast<LPARAM>(saved-1));
}
static void SpaceNativePropertyTabs(HWND root) {
    if (!g_settings.FlatPropertyPresentation || !IsCustomDarkModeAllowed() || IsWinUiTabStyleActive() || SysHighContrastEnabled() ||
        g_tabFeaturesUnloading.load(std::memory_order_acquire))return;
    HWND tab=FindWindowExW(root,nullptr,L"SysTabControl32",nullptr);
    if (!tab || GetWindowThreadProcessId(tab,nullptr)!=GetCurrentThreadId() ||
        (GetWindowLongPtrW(tab,GWL_STYLE)&(TCS_VERTICAL|TCS_BOTTOM|TCS_OWNERDRAWFIXED)))return;
    ApplyNativePropertyTabFont(tab);
    HFONT font=reinterpret_cast<HFONT>(SendMessageW(tab,WM_GETFONT,0,0));
    HDC dc=GetDC(tab);if(!dc)return;
    HGDIOBJ old=font?SelectObject(dc,font):nullptr;
    TEXTMETRICW metric{};const bool measured=GetTextMetricsW(dc,&metric);
    if(old)SelectObject(dc,old);ReleaseDC(tab,dc);
    if(!measured)return;
    RECT item{};if(!TabCtrl_GetItemRect(tab,0,&item))return;
    const int wanted=metric.tmHeight+MulDiv(10,GetDpiForWindow(tab),96);
    if(item.bottom-item.top>=wanted)return;
    const int extra=wanted-(item.bottom-item.top);
    for(HWND page=GetWindow(root,GW_CHILD);page;page=GetWindow(page,GW_HWNDNEXT)){
        if(!IsClassName(page,L"#32770") || !(GetWindowLongW(page,GWL_STYLE)&WS_VISIBLE))continue;
        RECT area{};GetClientRect(page,&area);
        for(HWND child=GetWindow(page,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;
            RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&bounds),2);
            if(bounds.bottom>area.bottom-extra-2)return;
        }
    }
    const LRESULT previous=SendMessageW(tab,TCM_SETITEMSIZE,0,MAKELPARAM(0,wanted));
    if(!GetPropW(tab,kNativeTabSavedSize) &&
        !SetPropW(tab,kNativeTabSavedSize,reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(previous)+1)))
        SendMessageW(tab,TCM_SETITEMSIZE,0,previous);
}
static void BalanceNativePropertyPage(HWND page, bool preparing = false) {
    if (!NativePropertyPresentationEnabled(page) || (!preparing && !IsWindowVisible(page))) return;
    // All state mutation and child movement happens on the page's UI thread.
    if (GetWindowThreadProcessId(page,nullptr)!=GetCurrentThreadId()) return;
    if(BalanceNativeDetailsPage(page))return;
    auto* state=reinterpret_cast<BalancedPageState*>(GetPropW(page,kBalancedPageState));
    if (state && state->applying) return;
    HWND root=GetParent(page),tab=FindWindowExW(root,nullptr,L"SysTabControl32",nullptr);
    RECT display{};GetClientRect(tab,&display);TabCtrl_AdjustRect(tab,FALSE,&display);
    MapWindowPoints(tab,root,reinterpret_cast<POINT*>(&display),2);
    RECT pageRect{};GetWindowRect(page,&pageRect);
    MapWindowPoints(nullptr,root,reinterpret_cast<POINT*>(&pageRect),2);
    if (pageRect.top<display.top && pageRect.bottom-display.top>50) {
        const int availableHeight=pageRect.bottom-display.top;
        for(HWND child=GetWindow(page,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;
            RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&bounds),2);
            if(bounds.bottom>availableHeight-2)return;
        }
        if(!state){state=new BalancedPageState;if(!SetPropW(page,kBalancedPageState,state)){delete state;return;}}
        if(!state->resized || !EqualRect(&pageRect,&state->assignedPage))state->originalPage=pageRect;
        state->applying=true;
        if(SetWindowPos(page,nullptr,pageRect.left,display.top,pageRect.right-pageRect.left,
                pageRect.bottom-display.top,SWP_NOZORDER|SWP_NOACTIVATE)){
            state->resized=true;state->assignedPage=pageRect;state->assignedPage.top=display.top;
        }
        state->applying=false;
    }
    RECT client{};if (!GetClientRect(page,&client)) return;
    RECT bounds{};bool found=false;
    std::vector<HWND> children;
    for (HWND child=GetWindow(page,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        const LONG_PTR style=GetWindowLongPtrW(child,GWL_STYLE);
        if (!(style&WS_VISIBLE) || IsClassName(child,L"ScrollBar")) continue;
        RECT rect{};if (!GetWindowRect(child,&rect))continue;
        MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&rect),2);
        if (rect.right<=rect.left || rect.bottom<=rect.top)continue;
        // Scrolling/full-page hosts own their own layout; moving them would
        // shift a viewport rather than balance a compact group of controls.
        if (rect.right-rect.left>=client.right-4 && rect.bottom-rect.top>=client.bottom-4)return;
        if (!found)bounds=rect;else UnionRect(&bounds,&bounds,&rect);
        found=true;children.push_back(child);
    }
    if (!found || children.empty())return;
    const int margin=(std::max)(4,MulDiv(12,GetDpiForWindow(page),96));
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    // Never shrink, clip or scale content to make it fit.
    if (width>client.right-2*margin || height>client.bottom-2*margin)return;
    const int dx=(client.right-width)/2-bounds.left;
    const int dy=(client.bottom-height)/2-bounds.top;
    if (!dx && !dy)return;
    if (!state) {
        state=new BalancedPageState;
        if (!SetPropW(page,kBalancedPageState,state)){delete state;return;}
    }
    state->applying=true;
    for (HWND child:children) {
        const POINT current=BalancedChildPoint(child,page);
        auto previous=state->children.find(child);
        if (previous==state->children.end() || GetPropW(child,kBalancedChildOwner)!=state) {
            if (!SetPropW(child,kBalancedChildOwner,state))continue;
            previous=state->children.insert_or_assign(child,BalancedChildPosition{current,current}).first;
        } else if (current.x!=previous->second.assigned.x || current.y!=previous->second.assigned.y) {
            previous->second.original=current;
        }
        if (SetWindowPos(child,nullptr,current.x+dx,current.y+dy,0,0,
                SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE))
            previous->second.assigned={current.x+dx,current.y+dy};
    }
    state->applying=false;
}

