// Windows known-folder Location page: add space between its icon and caption
// without translating any other row, changing the target, or matching titles.
static constexpr wchar_t kFolderLocationCaptionState[] = L"W32M.LocationCaption";
static constexpr wchar_t kFolderLocationCaptionOwner[] = L"W32M.LocationCaptionOwner";
struct FolderLocationCaptionState { HWND label; RECT original, assigned; };
static RECT FolderLocationChildRect(HWND child, HWND page) {
    RECT rect{}; GetWindowRect(child, &rect);
    MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&rect), 2);
    return rect;
}
static void RestoreFolderLocationCaption(HWND page) {
    auto* state = reinterpret_cast<FolderLocationCaptionState*>(
        RemovePropW(page, kFolderLocationCaptionState));
    if (!state) return;
    if (IsWindow(state->label) && GetParent(state->label) == page &&
        GetPropW(state->label, kFolderLocationCaptionOwner) == state) {
        RemovePropW(state->label, kFolderLocationCaptionOwner);
        RECT current = FolderLocationChildRect(state->label, page);
        if (EqualRect(&current, &state->assigned))
            SetWindowPos(state->label, nullptr, state->original.left,
                state->original.top, state->original.right-state->original.left,
                state->original.bottom-state->original.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
    }
    delete state;
}
static void SpaceFolderLocationCaption(HWND page) {
    if (!IsCustomDarkModeAllowed() || IsWinUiTabStyleActive() ||
        SysHighContrastEnabled() || !IsClassName(page,L"#32770") ||
        !(GetWindowLongW(page,GWL_STYLE)&WS_CHILD) || !HasTabControlSibling(page) ||
        GetPropW(page,kFolderLocationCaptionState)) return;
    HWND icon=GetDlgItem(page,401), label=GetDlgItem(page,413);
    if (!IsClassName(icon,L"Static") || !IsClassName(label,L"Static") ||
        (GetWindowLongW(icon,GWL_STYLE)&SS_TYPEMASK)!=SS_ICON ||
        !IsClassName(GetDlgItem(page,406),L"Static") ||
        !IsClassName(GetDlgItem(page,407),L"Edit") ||
        !IsClassName(GetDlgItem(page,408),L"Button") ||
        !IsClassName(GetDlgItem(page,409),L"Button") ||
        !IsClassName(GetDlgItem(page,414),L"Button")) return;
    const RECT image=FolderLocationChildRect(icon,page);
    const RECT original=FolderLocationChildRect(label,page);
    const int gap=MulDiv(10,GetDpiForWindow(page),96);
    const int left=image.right+gap;
    if (original.left>=left || original.left<image.right-gap ||
        original.bottom<=image.top || original.top>=image.bottom ||
        original.right-left<MulDiv(120,GetDpiForWindow(page),96)) return;
    wchar_t text[1024]{};
    const int count=GetWindowTextW(label,text,ARRAYSIZE(text));
    if(count<=0 || count>=static_cast<int>(ARRAYSIZE(text))-1)return;
    HDC dc=GetDC(label);if(!dc)return;
    HFONT font=reinterpret_cast<HFONT>(SendMessageW(label,WM_GETFONT,0,0));
    HGDIOBJ old=font?SelectObject(dc,font):nullptr;
    RECT measure{0,0,original.right-left,0};
    UINT flags=DT_CALCRECT|DT_WORDBREAK;
    if(GetWindowLongW(label,GWL_STYLE)&SS_NOPREFIX)flags|=DT_NOPREFIX;
    DrawTextW(dc,text,count,&measure,flags);
    if(old)SelectObject(dc,old);ReleaseDC(label,dc);
    // Keep native line height and every following control in place.
    if(measure.bottom-measure.top>original.bottom-original.top)return;
    RECT assigned=original;assigned.left=left;
    auto* state=new FolderLocationCaptionState{label,original,assigned};
    if(!SetPropW(page,kFolderLocationCaptionState,state)){delete state;return;}
    if(!SetPropW(label,kFolderLocationCaptionOwner,state)){
        RemovePropW(page,kFolderLocationCaptionState);delete state;return;
    }
    if(!SetWindowPos(label,nullptr,left,original.top,original.right-left,
        original.bottom-original.top,SWP_NOZORDER|SWP_NOACTIVATE))
        RestoreFolderLocationCaption(page);
}
