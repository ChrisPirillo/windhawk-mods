#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
static bool dark=true,custom=false,contrast=false;
struct { bool FlatPropertyPresentation=true; } g_settings;
static constexpr wchar_t kNativeFlatTabOwner[]=L"W32M.NativeFlatTabOwner";
static constexpr wchar_t kNativePropertyPrepared[]=L"W32M.PropertyPrepared";
static std::atomic<bool> g_tabFeaturesUnloading=false,g_nativeUiFontUnloading=false;
static HWND g_tlsPaintHwnd;
static constexpr LPCWSTR kPropertyDialogDarkModeMarker=L"_PropDkApplied";
static bool IsCustomDarkModeAllowed(){return dark;}
static bool IsWinUiTabStyleActive(){return custom;}
static bool SysHighContrastEnabled(){return contrast;}
static bool IsClassName(HWND window,LPCWSTR wanted){wchar_t name[80]{};return GetClassNameW(window,name,80) && !_wcsicmp(name,wanted);}
static bool HasTabControlSibling(HWND page){return FindWindowExW(GetParent(page),nullptr,WC_TABCONTROLW,nullptr)!=nullptr;}
static thread_local bool t_nativePropertyFontUpdate=false;
static UINT g_nativeUiFontSyncMessage=WM_APP+42;
namespace WindhawkUtils {
static bool SetWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* fn)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR),DWORD_PTR data);
static bool RemoveWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* fn)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR));
}
static LRESULT CALLBACK FontBridge(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR d){auto fn=reinterpret_cast<LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR)>(id);return fn(w,m,a,b,d);}
namespace WindhawkUtils {
static bool SetWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* fn)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR),DWORD_PTR d){return SetWindowSubclass(w,FontBridge,reinterpret_cast<UINT_PTR>(fn),d);}
static bool RemoveWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* fn)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR)){return RemoveWindowSubclass(w,FontBridge,reinterpret_cast<UINT_PTR>(fn));}
}
static bool FlatNativePropertyChrome(HWND w){return g_settings.FlatPropertyPresentation&&dark&&!custom&&!contrast&&GetPropW(GetAncestor(w,GA_ROOT),kPropertyDialogDarkModeMarker);}
#ifdef REJECTED_TAB_WIDTH
#include "flat-static-repair/tab-font-rejected.inc"
#else
#include "native-property-tab-font.inc"
#endif
#include "folder-location-spacing.inc"
#ifdef REJECTED_DETAILS_LAYOUT
static constexpr wchar_t kNativeDetailsLayout[]=L"W32M.DetailsLayout";
static RECT NativeDetailsRect(HWND window,HWND page){RECT r{};GetWindowRect(window,&r);MapWindowPoints(nullptr,page,(POINT*)&r,2);return r;}
#include "native-labels-repair/balance-before-details.inc"
#elif defined(REJECTED_DETAILS_AXIS)
#include "native-boundaries-repair/balance-axis-rejected.inc"
#else
#include "flat-static-repair/balance-exact.inc"
#endif
struct FontCache {
    std::vector<HFONT> fonts;
    HFONT Ensure(const LOGFONTW& description){HFONT font=CreateFontIndirectW(&description);if(font)fonts.push_back(font);return font;}
    ~FontCache(){for(HFONT font:fonts)DeleteObject(font);}
} g_winUiTabFontCache;
static std::wstring GetCachedThemeClass(HTHEME){return L"Tab";}
static LONG drawnWeight; static SIZE captionExtent; static int drawnBaseline;
static RECT drawnRect;
static HRESULT WINAPI DrawTextProbe(HTHEME,HDC dc,int,int,LPCWSTR text,int count,DWORD flags,LPRECT rect,const DTTOPTS*){
    LOGFONTW font{};GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font);drawnWeight=font.lfWeight;drawnRect=*rect;TEXTMETRICW tm{};GetTextMetricsW(dc,&tm);drawnBaseline=rect->top+tm.tmAscent;GetTextExtentPoint32W(dc,text,count<0?lstrlenW(text):count,&captionExtent);
    DrawTextW(dc,text,count,rect,flags);return S_OK;
}
static auto DrawThemeTextEx_orig=DrawTextProbe;
static auto DrawTextW_orig=DrawTextW;
static auto DrawTextExW_orig=DrawTextExW;
#include "native-property-caption.inc"
static constexpr COLORREF Win32MainDark=RGB(32,32,32);
static HBRUSH GetCachedSolidBrush(COLORREF c){static HBRUSH normal=CreateSolidBrush(RGB(32,32,32)),hover=CreateSolidBrush(RGB(48,48,48));return c==RGB(48,48,48)?hover:normal;}
static auto FillRect_orig=FillRect;
#include "flat-static-repair/tab-background-exact.inc"
int main(){
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_TAB_CLASSES};InitCommonControlsEx(&common);
    unsigned failures=0;auto check=[&](bool okay,const char* label){printf("%s %s\n",okay?"PASS":"FAIL",label);if(!okay)failures++;};
    HWND root=CreateWindowExW(0,L"#32770",L"Own property layout fixture",WS_POPUP|WS_VISIBLE,-30000,-30000,420,440,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND tab=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE,0,0,420,400,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<LPWSTR>(L"Sharing");TabCtrl_InsertItem(tab,0,&item);
    item.pszText=const_cast<LPWSTR>(L"Security");TabCtrl_InsertItem(tab,1,&item);
    HWND page=CreateWindowExW(0,L"#32770",L"",WS_CHILD|WS_VISIBLE,4,24,410,370,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND label=CreateWindowExW(0,L"STATIC",L"First label",WS_CHILD|WS_VISIBLE,12,10,140,22,page,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND edit=CreateWindowExW(0,L"EDIT",L"Own editable sample",WS_CHILD|WS_VISIBLE,12,50,280,24,page,nullptr,GetModuleHandleW(nullptr),nullptr);
    HFONT font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(tab,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
    const POINT originalLabel=BalancedChildPoint(label,page),originalEdit=BalancedChildPoint(edit,page);
    RECT originalItem{};TabCtrl_GetItemRect(tab,0,&originalItem);
    SpaceNativePropertyTabs(root);RECT roomy{};TabCtrl_GetItemRect(tab,0,&roomy);
    check(roomy.bottom-roomy.top>=26,"tab height reserves font height plus breathing room");
    BalanceNativePropertyPage(page);
    POINT movedLabel=BalancedChildPoint(label,page),movedEdit=BalancedChildPoint(edit,page);
    RECT client{};GetClientRect(page,&client);
    check(abs(movedLabel.x-(client.right-280)/2)<=1 && abs(movedLabel.y-(client.bottom-64)/2)<=1,"compact content group is centered horizontally and vertically");
    check(movedEdit.x-movedLabel.x==originalEdit.x-originalLabel.x && movedEdit.y-movedLabel.y==originalEdit.y-originalLabel.y,"relative control alignment and spacing preserved");
    BalanceNativePropertyPage(page);POINT repeated=BalancedChildPoint(label,page);
    check(repeated.x==movedLabel.x && repeated.y==movedLabel.y,"repeated paint does not drift content");
    RestoreBalancedPage(page);POINT restored=BalancedChildPoint(label,page);
    check(restored.x==originalLabel.x && restored.y==originalLabel.y,"unload restores native child positions");
    RestoreNativePropertyTabSize(root);RECT restoredItem{};TabCtrl_GetItemRect(tab,0,&restoredItem);
    check(EqualRect(&originalItem,&restoredItem),"unload restores original tab item dimensions");
    contrast=true;BalanceNativePropertyPage(page);POINT highContrast=BalancedChildPoint(label,page);
    check(highContrast.x==originalLabel.x && highContrast.y==originalLabel.y,"high contrast leaves geometry native");contrast=false;
    SetWindowPos(label,nullptr,12,10,405,365,SWP_NOZORDER|SWP_NOACTIVATE);
    BalanceNativePropertyPage(page);POINT full=BalancedChildPoint(label,page);
    check(full.x==12 && full.y==10,"full-size content is never shrunk or repositioned to fit");
    SetWindowPos(label,nullptr,12,10,140,22,SWP_NOZORDER|SWP_NOACTIVATE);
    BalanceNativePropertyPage(page);SetWindowPos(label,nullptr,20,25,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);RestoreBalancedPage(page);
    POINT appPosition=BalancedChildPoint(label,page);check(appPosition.x==20 && appPosition.y==25,"unload preserves newer application layout changes");
    SetPropW(root,kPropertyDialogDarkModeMarker,reinterpret_cast<HANDLE>(1));g_tlsPaintHwnd=tab;
    HDC dc=GetDC(tab);HGDIOBJ old=SelectObject(dc,font);RECT itemRect{};TabCtrl_GetItemRect(tab,0,&itemRect);
    check(DrawNativePropertyTabCaption(reinterpret_cast<HTHEME>(1),dc,TABP_TABITEM,TIS_SELECTED,L"Sharing",7,0,&itemRect,nullptr) && drawnWeight==FW_BOLD,"selected native tab label is bold");
    check(drawnRect.bottom==itemRect.bottom-1,"caption retains lower descender clearance");
    const int selectedBaseline=drawnBaseline;
    TabCtrl_SetCurSel(tab,1);
    DrawNativePropertyTabCaption(reinterpret_cast<HTHEME>(1),dc,TABP_TABITEM,TIS_NORMAL,L"Sharing",7,0,&itemRect,nullptr);
    check(drawnBaseline==selectedBaseline,"changing selection keeps the caption baseline fixed");
    TabCtrl_SetCurSel(tab,0);
    check(DrawNativePropertyTabCaption(reinterpret_cast<HTHEME>(1),dc,TABP_TABITEM,TIS_NORMAL,L"Sharing",7,0,&itemRect,nullptr) && drawnWeight==FW_NORMAL,"unselected native tab label is regular");
    check(!DrawNativePropertyTabCaption(reinterpret_cast<HTHEME>(1),dc,TABP_TABITEM,TIS_SELECTED,L"Sharing",7,DT_CALCRECT,&itemRect,nullptr),"measurement calls never paint or substitute geometry");
    SelectObject(dc,old);ReleaseDC(tab,dc);
    // Exercise the actual theme API as well as the argument-contract probe.
    // A mock alone cannot prove the API honors the selected font's weight.
    DrawThemeTextEx_orig=DrawThemeTextEx;
    ApplyNativePropertyTabFont(tab);SpaceNativePropertyTabs(root);TabCtrl_GetItemRect(tab,0,&itemRect);    LOGFONTW beforeShape{},afterShape{};
    GetObjectW(font,sizeof(beforeShape),&beforeShape);
    GetObjectW((HFONT)SendMessageW(tab,WM_GETFONT,0,0),sizeof(afterShape),&afterShape);
    check(afterShape.lfWidth==beforeShape.lfWidth && afterShape.lfHeight==beforeShape.lfHeight &&
          afterShape.lfEscapement==beforeShape.lfEscapement && afterShape.lfOrientation==beforeShape.lfOrientation,
          "tab role font preserves natural character width, height and orientation");
    HTHEME theme=OpenThemeData(tab,L"Tab");
    HDC pixels=CreateCompatibleDC(nullptr);BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=420;info.bmiHeader.biHeight=-80;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(pixels,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    HGDIOBJ oldBitmap=SelectObject(pixels,bitmap),oldPixelFont=SelectObject(pixels,font);SetBkMode(pixels,TRANSPARENT);
    RECT surface{0,0,100,40};
    check(PaintNativePropertyTabBackground(pixels,TABP_TABITEM,TIS_NORMAL,&surface) && GetPixel(pixels,10,10)==RGB(32,32,32),"normal flat tab blends with the page background");
    check(PaintNativePropertyTabBackground(pixels,TABP_TABITEM,TIS_HOT,&surface) && GetPixel(pixels,10,10)==RGB(48,48,48),"hovered flat tab has a restrained grey surface");
    check(PaintNativePropertyTabBackground(pixels,TABP_TABITEM,TIS_SELECTED,&surface) && GetPixel(pixels,0,0)==RGB(32,32,32),"selected tab outline blends with the page");
    auto ink=[&](int state){memset(bits,0,420*80*4);bool okay=DrawNativePropertyTabCaption(theme,pixels,TABP_TABITEM,state,L"Sharing",7,0,&itemRect,nullptr);unsigned count=0;for(int i=0;i<420*80;i++)if((static_cast<DWORD*>(bits)[i]&0xFFFFFF)!=0)count++;return okay?count:0;};
    unsigned normalInk=ink(TIS_NORMAL),boldInk=ink(TIS_SELECTED);
    check(normalInk>0 && boldInk>normalInk,"real theme API renders more ink for selected bold caption");
    bool bottomClear=true;for(int x=itemRect.left;x<itemRect.right;x++)if(static_cast<DWORD*>(bits)[(itemRect.bottom-1)*420+x]&0xFFFFFF)bottomClear=false;
    check(bottomClear,"descender has clear space below its rendered pixels");
    SelectObject(pixels,oldPixelFont);SelectObject(pixels,oldBitmap);DeleteObject(bitmap);DeleteDC(pixels);CloseThemeData(theme);
    DrawThemeTextEx_orig=DrawTextProbe;
    TabCtrl_DeleteAllItems(tab);
    for(LPCWSTR name:{L"General",L"Sharing",L"Security",L"Location",L"Previous Versions"}){TCITEMW entry{};entry.mask=TCIF_TEXT;entry.pszText=(LPWSTR)name;TabCtrl_InsertItem(tab,TabCtrl_GetItemCount(tab),&entry);}
    HDC measureDC=GetDC(tab);HGDIOBJ priorFont=SelectObject(measureDC,(HFONT)SendMessageW(tab,WM_GETFONT,0,0));bool labelsFit=true;
    for(int i=0;i<TabCtrl_GetItemCount(tab);i++){
        wchar_t name[80]{};TCITEMW entry{};entry.mask=TCIF_TEXT;entry.pszText=name;entry.cchTextMax=80;TabCtrl_GetItem(tab,i,&entry);RECT item{};TabCtrl_GetItemRect(tab,i,&item);
        for(int state:{TIS_NORMAL,TIS_SELECTED}){
            bool painted=DrawNativePropertyTabCaption((HTHEME)1,measureDC,TABP_TABITEM,state,name,lstrlenW(name),0,&item,nullptr);
            if(!painted || captionExtent.cx+10>drawnRect.right-drawnRect.left){labelsFit=false;printf("caption-fit item=%d state=%d painted=%d ink-width=%ld available=%ld item-width=%ld\n",i,state,painted,captionExtent.cx,drawnRect.right-drawnRect.left,item.right-item.left);}
        }
    }
    check(labelsFit,"all five real Properties tab names retain horizontal breathing room at both weights");
    SelectObject(measureDC,priorFont);ReleaseDC(tab,measureDC);
        RemoveNativePropertyTabFont(tab);
    printf("font original=%p current=%p state=%p\n",font,(HFONT)SendMessageW(tab,WM_GETFONT,0,0),GetPropW(tab,kNativePropertyTabFont));check((HFONT)SendMessageW(tab,WM_GETFONT,0,0)==font,"role font removal restores original font handle");
    ApplyNativePropertyTabFont(tab);
    HFONT newer=CreateFontW(-18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");
    SendMessageW(tab,WM_SETFONT,(WPARAM)newer,FALSE);
    HFONT role=(HFONT)SendMessageW(tab,WM_GETFONT,0,0);LOGFONTW roleDesc{};GetObjectW(role,sizeof(roleDesc),&roleDesc);
    printf("role=%p newer=%p height=%ld weight=%ld face=%ls\n",role,newer,roleDesc.lfHeight,roleDesc.lfWeight,roleDesc.lfFaceName);check(role!=newer && roleDesc.lfWeight==700 && roleDesc.lfHeight==-18 && !_wcsicmp(roleDesc.lfFaceName,L"Tahoma"),"new application font preserves family and size while refreshing tab weight");
    RemoveNativePropertyTabFont(tab);
    check((HFONT)SendMessageW(tab,WM_GETFONT,0,0)==newer,"unload preserves newer application font");
    g_tabFeaturesUnloading=true;ApplyNativePropertyTabFont(tab);
    check(!GetPropW(tab,kNativePropertyTabFont),"unload prevents role font reapplication");
    g_settings.FlatPropertyPresentation=false;
    HDC nativeDC=GetDC(tab);RECT nativeItem{};TabCtrl_GetItemRect(tab,0,&nativeItem);
    check(!DrawNativePropertyTabCaption((HTHEME)1,nativeDC,TABP_TABITEM,TIS_SELECTED,L"General",7,0,&nativeItem,nullptr),"original tab modes bypass the flat caption painter");
    ReleaseDC(tab,nativeDC);
    g_tabFeaturesUnloading=false;g_settings.FlatPropertyPresentation=true;
    INITCOMMONCONTROLSEX detailsCommon{sizeof(detailsCommon),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&detailsCommon);
    HWND details=CreateWindowExW(0,L"#32770",L"",WS_CHILD|WS_VISIBLE,0,30,410,370,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND detailsList=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT,5,5,400,310,details,(HMENU)271,GetModuleHandleW(nullptr),nullptr);
    HWND detailsFooter=CreateWindowExW(0,L"STATIC",L"Remove sample metadata",WS_CHILD|WS_VISIBLE,5,330,300,22,details,(HMENU)272,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(detailsList,WM_SETFONT,(WPARAM)font,FALSE);
    LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=(LPWSTR)L"Property";col.cx=130;ListView_InsertColumn(detailsList,0,&col);
    col.pszText=(LPWSTR)L"Value";col.cx=260;ListView_InsertColumn(detailsList,1,&col);
    LVGROUP group{};group.cbSize=sizeof(group);group.mask=LVGF_HEADER|LVGF_GROUPID;group.pszHeader=(LPWSTR)L"File";group.iGroupId=1;ListView_InsertGroup(detailsList,-1,&group);ListView_EnableGroupView(detailsList,TRUE);
    for(int i=0;i<5;i++){LVITEMW row{};row.mask=LVIF_TEXT|LVIF_GROUPID;row.iItem=i;row.iGroupId=1;row.pszText=(LPWSTR)L"Property name";ListView_InsertItem(detailsList,&row);ListView_SetItemText(detailsList,i,1,(LPWSTR)L"Local value");}
    ListView_SetItemState(detailsList,2,LVIS_SELECTED,LVIS_SELECTED);
    const RECT detailsOriginal=NativeDetailsRect(detailsList,details),footerOriginal=NativeDetailsRect(detailsFooter,details);
    BalanceNativePropertyPage(details);
    printf("details state=%p groups=%lld top=%d\n",GetPropW(details,kNativeDetailsLayout),(long long)SendMessageW(detailsList,LVM_GETGROUPCOUNT,0,0),ListView_GetTopIndex(detailsList));
    RECT compactList=NativeDetailsRect(detailsList,details),compactFooter=NativeDetailsRect(detailsFooter,details),detailsArea{};GetClientRect(details,&detailsArea);
    printf("details rect %ld,%ld,%ld,%ld footer %ld,%ld,%ld,%ld area %ld,%ld\n",compactList.left,compactList.top,compactList.right,compactList.bottom,compactFooter.left,compactFooter.top,compactFooter.right,compactFooter.bottom,detailsArea.right,detailsArea.bottom);
    check(GetPropW(details,kNativeDetailsLayout)!=nullptr && abs(compactList.left+compactList.right-detailsArea.right)<=1 &&
        abs(compactList.top+compactFooter.bottom-detailsArea.bottom)<=1,"Details list and footer form a centered measured content group");
    RECT rowBounds{},detailsClient{};ListView_GetItemRect(detailsList,4,&rowBounds,LVIR_BOUNDS);GetClientRect(detailsList,&detailsClient);
    check(rowBounds.bottom<=detailsClient.bottom && ListView_GetColumnWidth(detailsList,0)+ListView_GetColumnWidth(detailsList,1)<=detailsClient.right,"all Details rows and measured text columns fit without clipping");
    check(ListView_GetItemState(detailsList,2,LVIS_SELECTED)==LVIS_SELECTED && ListView_GetItemCount(detailsList)==5,"Details keeps native item data and selection");
    BalanceNativePropertyPage(details);RECT repeatedDetails=NativeDetailsRect(detailsList,details);
    check(EqualRect(&compactList,&repeatedDetails),"Details preparation never drifts on repeated paint");
    RestoreBalancedPage(details);RECT restoredDetails=NativeDetailsRect(detailsList,details),restoredFooter=NativeDetailsRect(detailsFooter,details);
    check(EqualRect(&restoredDetails,&detailsOriginal)&&EqualRect(&restoredFooter,&footerOriginal)&&ListView_GetColumnWidth(detailsList,0)==130&&ListView_GetColumnWidth(detailsList,1)==260,"Details unload restores native viewport, footer and column widths");
    std::wstring longMetadata(300,L'W');ListView_SetItemText(detailsList,0,1,longMetadata.data());BalanceNativePropertyPage(details);
    RECT largeDetails=NativeDetailsRect(detailsList,details);
    check(GetPropW(details,kNativeDetailsLayout) && largeDetails.right-largeDetails.left==detailsOriginal.right-detailsOriginal.left && ListView_GetColumnWidth(detailsList,0)==130 && ListView_GetColumnWidth(detailsList,1)==260,"long Details values retain native width and columns while centering vertically");
    RECT longFooter=NativeDetailsRect(detailsFooter,details);check(abs(largeDetails.top+longFooter.bottom-detailsArea.bottom)<=1,"long-value Details group is vertically centered instead of rejected by its width");
    RestoreBalancedPage(details);
    ListView_SetItemText(detailsList,0,1,(LPWSTR)L"Local value");BalanceNativePropertyPage(details);
    SetWindowPos(detailsList,nullptr,20,25,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);ListView_SetColumnWidth(detailsList,0,175);RestoreBalancedPage(details);
    RECT appDetails=NativeDetailsRect(detailsList,details);
    check(appDetails.left==20&&appDetails.top==25&&ListView_GetColumnWidth(detailsList,0)==175,"Details restoration preserves newer application layout and column choices");
    DestroyWindow(details);
    RestoreNativePropertyTabSize(root);DestroyWindow(root);DeleteObject(font);DeleteObject(newer);
    return failures?1:0;
}






