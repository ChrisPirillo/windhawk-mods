#define main prior_presentation_main
#include "../property-presentation-check.cpp"
#undef main
#include <windowsx.h>
static thread_local bool g_tlsPaintWasInsideExplorer=false,g_tlsExplorerMicaPaintTrackingEnabled=false;
#include "paint-owner.inc"

static unsigned ownerCalls,baselineErrors,weightErrors,memoryOwnerFailures;
static unsigned activationAttempts;
static LRESULT CALLBACK ObserveOwnActivation(int code,WPARAM wp,LPARAM lp){
 if(code==HCBT_ACTIVATE){activationAttempts++;return 1;}
 return CallNextHookEx(nullptr,code,wp,lp);
}
static int commonBaseline=-1;static HWND actualTab;static int drawingItem=-1;
static int WINAPI TextRecord(HDC dc,LPCWSTR text,int count,RECT* rect,UINT flags){
 TEXTMETRICW metric{};GetTextMetricsW(dc,&metric);int baseline=rect->top+metric.tmAscent;
 printf("owner item=%d selected=%d top=%ld bottom=%ld ascent=%ld baseline=%d\n",drawingItem,TabCtrl_GetCurSel(actualTab),rect->top,rect->bottom,metric.tmAscent,baseline);if(commonBaseline<0)commonBaseline=baseline;else if(commonBaseline!=baseline)baselineErrors++;
 LOGFONTW font{};GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font);
 if(font.lfWeight!=(drawingItem==TabCtrl_GetCurSel(actualTab)?FW_BOLD:FW_NORMAL))weightErrors++;
 return DrawTextW(dc,text,count,rect,flags);
}
#ifdef REJECTED_FLAT_TAB_MODE
#include "../mode-boundaries-repair/owner-before-mode-fix.inc"
#elif defined(REJECTED_FLAT_TAB_ROW)
#include "../native-boundaries-repair/flat-tab-owner-rejected.inc"
#else
#include "owner.inc"
#endif
#include "../flat-property-rules.inc"
static LRESULT CALLBACK ParentDraw(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR){
 if(m==WM_DRAWITEM){auto item=(DRAWITEMSTRUCT*)b;RECT nativePart{0,0,30,2};if(!PaintNativePropertyTabBackground(item->hDC,TABP_TOPTABITEM,TIS_NORMAL,&nativePart))memoryOwnerFailures++;drawingItem=item->itemID;if(NativeFlatTabDrawItem(w,item)){ownerCalls++;return TRUE;}}
 return DefSubclassProc(w,m,a,b);
}
int main(){
 INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_TAB_CLASSES};InitCommonControlsEx(&controls);
 HHOOK activationObserver=SetWindowsHookExW(WH_CBT,ObserveOwnActivation,nullptr,GetCurrentThreadId());
 HWND foreground=GetForegroundWindow();HWND root=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"#32770",L"Own flat tab rendering fixture",WS_POPUP,-30000,-30000,420,300,0,0,GetModuleHandleW(0),0);
 SetPropW(root,kPropertyDialogDarkModeMarker,(HANDLE)1);SetWindowSubclass(root,ParentDraw,1,0);
 actualTab=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE,0,0,400,260,root,(HMENU)12320,GetModuleHandleW(0),0);
 const wchar_t* names[]={L"General",L"Digital Signatures",L"Security",L"Details",L"Previous Versions"};TCITEMW value{};value.mask=TCIF_TEXT;
 for(auto name:names){value.pszText=(LPWSTR)name;TabCtrl_InsertItem(actualTab,TabCtrl_GetItemCount(actualTab),&value);}
 HFONT font=CreateFontW(-15,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI Variable Text");SendMessageW(actualTab,WM_SETFONT,(WPARAM)font,0);
 ApplyNativePropertyTabFont(actualTab);SendMessageW(actualTab,TCM_SETITEMSIZE,0,MAKELPARAM(0,30));
 LONG_PTR original=GetWindowLongPtrW(actualTab,GWL_STYLE);InstallNativeFlatTab(actualTab);
 unsigned errors=0;auto check=[&](bool b,const char*n){printf("%s %s\n",b?"PASS":"FAIL",n);errors+=!b;};
 check(GetPropW(actualTab,kNativeFlatTabOwner)!=nullptr&&(GetWindowLongPtrW(actualTab,GWL_STYLE)&TCS_OWNERDRAWFIXED),"Flat takes explicit ownership of native tab caption painting");
 HDC dc=CreateCompatibleDC(0);HBITMAP bitmap=CreateBitmap(420,300,1,32,0);auto old=SelectObject(dc,bitmap);DrawTextW_orig=TextRecord; g_tlsPaintHwnd=actualTab;RECT pane{0,32,400,260};check(PaintNativePropertyTabBackground(dc,TABP_PANE,0,&pane),"Flat pane keeps background-only painting with owned captions");
 g_tlsPaintHwnd=root;
 for(int i=0;i<5;i++){TabCtrl_SetCurSel(actualTab,i);SendMessageW(actualTab,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);}
 check(ownerCalls>=15,"real native tab control dispatches each caption through DRAWITEM");
 check(memoryOwnerFailures==0,"buffered tab backgrounds retain the live tab owner despite an old fallback window");
 check(g_tlsPaintHwnd==root,"tab print restores its enclosing caller paint owner");
 check(commonBaseline>=0&&baselineErrors==0,"all actual selected and unselected caption paints share one baseline");
 check(weightErrors==0&&ownerCalls>0,"actual selected caption is bold and unselected captions are regular");
 auto* state=(NativeFlatTabState*)GetPropW(actualTab,kNativeFlatTabOwner);RECT first{};TabCtrl_GetItemRect(actualTab,0,&first);
 SendMessageW(actualTab,WM_MOUSEMOVE,0,MAKELPARAM((first.left+first.right)/2,(first.top+first.bottom)/2));
 check(state->hot==0,"hover uses the native hit test rather than another caption layout");SendMessageW(actualTab,WM_MOUSELEAVE,0,0);check(state->hot==-1,"mouse leave clears hover state");
 InstallNativeFlatTab(actualTab);check(GetPropW(actualTab,kNativeFlatTabOwner)==state,"repeated preparation preserves one owner");
 contrast=true;SendMessageW(actualTab,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);
 check(!(GetWindowLongPtrW(actualTab,GWL_STYLE)&TCS_OWNERDRAWFIXED)&&!GetPropW(actualTab,kNativeFlatTabOwner),"contrast transition retires owned drawing before the native tab paints");
 check((HFONT)SendMessageW(actualTab,WM_GETFONT,0,0)==font,"contrast transition restores application font before native captions paint");
 contrast=false;ApplyNativePropertyTabFont(actualTab);InstallNativeFlatTab(actualTab);
 check(GetPropW(actualTab,kNativeFlatTabOwner)!=nullptr,"Flat captions can acquire ownership again after contrast ends");
 for(int mode=0;mode<3;mode++){
  if(mode==0)dark=false;else if(mode==1)custom=true;else g_settings.FlatPropertyPresentation=false;
  SendMessageW(actualTab,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);
  check(!GetPropW(actualTab,kNativeFlatTabOwner)&&!(GetWindowLongPtrW(actualTab,GWL_STYLE)&TCS_OWNERDRAWFIXED)&&(HFONT)SendMessageW(actualTab,WM_GETFONT,0,0)==font,mode==0?"light mode restores native drawing and application font":mode==1?"original WinUI mode restores native drawing and application font":"Native menu mode restores native drawing and application font");
  dark=true;custom=false;g_settings.FlatPropertyPresentation=true;ApplyNativePropertyTabFont(actualTab);InstallNativeFlatTab(actualTab);
 }
 RemoveNativeFlatTab(actualTab);check(GetWindowLongPtrW(actualTab,GWL_STYLE)==original&&!GetPropW(actualTab,kNativeFlatTabOwner),"unload restores the exact original tab style");
 check(TabCtrl_GetCurSel(actualTab)==4&&TabCtrl_GetItemCount(actualTab)==5,"native selection and tab items survive restore");
 g_settings.FlatPropertyPresentation=false;InstallNativeFlatTab(actualTab);check(!GetPropW(actualTab,kNativeFlatTabOwner),"original menu modes do not acquire custom drawing");g_settings.FlatPropertyPresentation=true;
 HWND line=CreateWindowExW(0,L"Static",L"",WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ,10,100,300,2,root,0,GetModuleHandleW(0),0);
 HWND label=CreateWindowExW(0,L"Static",L"Actual label",WS_CHILD|WS_VISIBLE,10,110,300,20,root,0,GetModuleHandleW(0),0);
 HBRUSH sentinel=CreateSolidBrush(RGB(160,160,160));RECT strip{0,0,300,2};FillRect(dc,&strip,sentinel);
 InstallFlatPropertyRuleChild(line,0);SendMessageW(line,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_NONCLIENT);
 check(GetPropW(line,kFlatPropertyRule)&&GetPixel(dc,100,0)==Win32MainDark&&GetPixel(dc,100,1)==Win32MainDark,"actual etched General-page separator paints only the page background");
 check(!FlatPropertyRuleEligible(label),"ordinary static text is never treated as a separator");
 RemoveFlatPropertyRules(root);check(!GetPropW(line,kFlatPropertyRule),"restoration removes decorative-line ownership");DeleteObject(sentinel);
 InstallNativeFlatTab(actualTab);DestroyWindow(actualTab);
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);DestroyWindow(root);DeleteObject(font);
 DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
 check(activationObserver&&activationAttempts==0&&foregroundPid!=GetCurrentProcessId(),"memory-only native-control check never attempts activation or owns foreground");
 if(GetForegroundWindow()!=foreground)printf("External foreground changed during test; own activation attempts=%u\n",activationAttempts);
 if(activationObserver)UnhookWindowsHookEx(activationObserver);return errors?1:0;
}
