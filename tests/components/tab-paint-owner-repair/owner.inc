// Flat tabs keep the native control's layout, hit testing and scrolling.
// DRAWITEM supplies documented tab-client coordinates and an exact item ID.
struct NativeFlatTabState { int hot=-1; bool tracking=false; RECT row{}; bool rowValid=false; unsigned paintDepth=0; };
static void RefreshNativeFlatTabRow(HWND tab,NativeFlatTabState* state){
    const int count=TabCtrl_GetItemCount(tab);state->rowValid=count>0&&TabCtrl_GetRowCount(tab)==1&&TabCtrl_GetItemRect(tab,0,&state->row);
    for(int i=1;i<count&&state->rowValid;i++){RECT peer{};state->rowValid=TabCtrl_GetItemRect(tab,i,&peer)!=FALSE;
        state->row.top=(std::max)(state->row.top,peer.top);state->row.bottom=(std::min)(state->row.bottom,peer.bottom);}
}
static bool NativeFlatTabDrawItem(HWND root,const DRAWITEMSTRUCT* item){
    if(!item || item->CtlType!=ODT_TAB || !item->hDC || GetParent(item->hwndItem)!=root ||
       !GetPropW(item->hwndItem,kNativeFlatTabOwner) || !FlatNativePropertyChrome(item->hwndItem) ||
       g_tabFeaturesUnloading.load(std::memory_order_acquire))return false;
    HWND tab=item->hwndItem;const int count=TabCtrl_GetItemCount(tab);
    if(item->itemID>=static_cast<UINT>(count) || TabCtrl_GetRowCount(tab)!=1)return false;
    wchar_t text[1024]{};TCITEMW value{};value.mask=TCIF_TEXT|TCIF_IMAGE;value.pszText=text;value.cchTextMax=1024;value.iImage=-1;
    if(!TabCtrl_GetItem(tab,item->itemID,&value) || value.iImage>=0)return false;
    auto* state=reinterpret_cast<NativeFlatTabState*>(GetPropW(tab,kNativeFlatTabOwner));
    if(!state->rowValid)return false;
    RECT row=item->rcItem;row.top=state->row.top;row.bottom=state->row.bottom;
    HFONT base=NativePropertyCaptionBaseFont(tab,reinterpret_cast<HFONT>(SendMessageW(tab,WM_GETFONT,0,0)));
    LOGFONTW description{};if(!base || !GetObjectW(base,sizeof(description),&description))return false;
    description.lfWeight=FW_NORMAL;HFONT regular=g_winUiTabFontCache.Ensure(description);
    description.lfWeight=FW_BOLD;HFONT bold=g_winUiTabFontCache.Ensure(description);if(!regular||!bold)return false;
    const int saved=SaveDC(item->hDC);if(!saved)return false;
    IntersectClipRect(item->hDC,item->rcItem.left,item->rcItem.top,item->rcItem.right,item->rcItem.bottom);
    const bool selected=TabCtrl_GetCurSel(tab)==static_cast<int>(item->itemID);
    HBRUSH brush=GetCachedSolidBrush(!selected&&state->hot==static_cast<int>(item->itemID)?RGB(48,48,48):Win32MainDark);
    FillRect_orig(item->hDC,&item->rcItem,brush);
    TEXTMETRICW normalMetric{},boldMetric{},metric{};
    SelectObject(item->hDC,regular);GetTextMetricsW(item->hDC,&normalMetric);
    SelectObject(item->hDC,bold);GetTextMetricsW(item->hDC,&boldMetric);
    const int ascent=(std::max)(normalMetric.tmAscent,boldMetric.tmAscent),descent=(std::max)(normalMetric.tmDescent,boldMetric.tmDescent);
    const int baseline=row.top+(row.bottom-row.top-ascent-descent)/2+ascent;
    SelectObject(item->hDC,selected?bold:regular);GetTextMetricsW(item->hDC,&metric);
    RECT caption{item->rcItem.left,baseline-metric.tmAscent,item->rcItem.right,row.bottom};
    SetTextColor(item->hDC,(item->itemState&ODS_DISABLED)?RGB(128,128,128):RGB(235,235,235));SetBkMode(item->hDC,TRANSPARENT);
    UINT flags=DT_SINGLELINE|DT_TOP|DT_CENTER;
    if(item->itemState&ODS_NOACCEL)flags|=DT_HIDEPREFIX;
    DrawTextW_orig(item->hDC,text,-1,&caption,flags);
    if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT)){
        RECT measured{0,0,0,0};DrawTextW_orig(item->hDC,text,-1,&measured,flags|DT_CALCRECT);
        const int width=(std::min)(measured.right-measured.left,caption.right-caption.left-4);
        const int left=(caption.left+caption.right-width)/2,y=(std::min)(static_cast<int>(row.bottom)-2,baseline+2);
        HBRUSH focus=GetCachedSolidBrush(RGB(112,112,112));
        for(int x=left;x<left+width;x+=2){RECT dot{x,y,x+1,y+1};FillRect_orig(item->hDC,&dot,focus);}
    }
    RestoreDC(item->hDC,saved);return true;
}
static void RemoveNativeFlatTab(HWND tab);
static LRESULT CALLBACK NativeFlatTabSubclass(HWND tab,UINT message,WPARAM wp,LPARAM lp,DWORD_PTR){
    auto* state=reinterpret_cast<NativeFlatTabState*>(GetPropW(tab,kNativeFlatTabOwner));
    if(message==WM_NCDESTROY){RemovePropW(tab,kNativeFlatTabOwner);WindhawkUtils::RemoveWindowSubclassFromAnyThread(tab,NativeFlatTabSubclass);delete state;return DefSubclassProc(tab,message,wp,lp);}
    const bool paint=message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT;
    // Native tab drawing must resume before forwarding the first paint in a
    // mode we do not own. Otherwise TCS_OWNERDRAWFIXED still dispatches to a
    // parent whose guarded DRAWITEM handler correctly declines that mode.
    if(state&&paint&&(!FlatNativePropertyChrome(tab)||g_tabFeaturesUnloading.load(std::memory_order_acquire))){
        RemoveNativeFlatTab(tab);RemoveNativePropertyTabFont(tab);
        RemovePropW(GetParent(tab),kNativePropertyPrepared);
        return DefSubclassProc(tab,message,wp,lp);
    }
    if(state && (message==WM_MOUSEMOVE || message==WM_MOUSELEAVE)){
        int hot=-1;
        if(message==WM_MOUSEMOVE){TCHITTESTINFO hit{};hit.pt={static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};hot=TabCtrl_HitTest(tab,&hit);
            if(!state->tracking){TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,tab,0};state->tracking=TrackMouseEvent(&track)!=FALSE;}}
        else state->tracking=false;
        if(hot!=state->hot){state->hot=hot;InvalidateRect(tab,nullptr,FALSE);}
    }
    // Buffered WM_PRINT paths have no BeginPaint, and g_tlsPaintHwnd can
    // otherwise name an earlier control. Bracket this exact native tab paint
    // so its theme backgrounds cannot fall back to a raised/light native edge.
    struct TabPaintOwnerScope {
        HWND window;bool completed;
        TabPaintOwnerScope(HWND w,bool actual):window(w),completed(actual){if(window)PaintOwnerEnter(window);}
        ~TabPaintOwnerScope(){if(window)PaintOwnerLeave(window,completed);}
    } ownerScope(state&&paint?tab:nullptr,message==WM_PAINT);
    if(state&&paint){if(!state->paintDepth)RefreshNativeFlatTabRow(tab,state);state->paintDepth++;}
    const LRESULT result=DefSubclassProc(tab,message,wp,lp);
    if(state&&paint&&GetPropW(tab,kNativeFlatTabOwner)==state)state->paintDepth--;
    return result;
}
static void RemoveNativeFlatTab(HWND tab){
    auto* state=reinterpret_cast<NativeFlatTabState*>(RemovePropW(tab,kNativeFlatTabOwner));if(!state)return;
    WindhawkUtils::RemoveWindowSubclassFromAnyThread(tab,NativeFlatTabSubclass);
    if(IsWindow(tab)){SetWindowLongPtrW(tab,GWL_STYLE,GetWindowLongPtrW(tab,GWL_STYLE)&~static_cast<LONG_PTR>(TCS_OWNERDRAWFIXED));InvalidateRect(tab,nullptr,TRUE);}
    delete state;
}
static void InstallNativeFlatTab(HWND tab){
    if(!FlatNativePropertyChrome(tab) || GetPropW(tab,kNativeFlatTabOwner) ||
       (GetWindowLongPtrW(tab,GWL_STYLE)&TCS_OWNERDRAWFIXED) ||
       g_tabFeaturesUnloading.load(std::memory_order_acquire))return;
    for(int i=0;i<TabCtrl_GetItemCount(tab);i++){TCITEMW item{};item.mask=TCIF_IMAGE;item.iImage=-1;if(TabCtrl_GetItem(tab,i,&item)&&item.iImage>=0)return;}
    auto* state=new NativeFlatTabState;
    if(!SetPropW(tab,kNativeFlatTabOwner,state)){delete state;return;}
    if(!WindhawkUtils::SetWindowSubclassFromAnyThread(tab,NativeFlatTabSubclass,0)){RemovePropW(tab,kNativeFlatTabOwner);delete state;return;}
    SetWindowLongPtrW(tab,GWL_STYLE,GetWindowLongPtrW(tab,GWL_STYLE)|TCS_OWNERDRAWFIXED);
    RefreshNativeFlatTabRow(tab,state);
    InvalidateRect(tab,nullptr,TRUE);
}

