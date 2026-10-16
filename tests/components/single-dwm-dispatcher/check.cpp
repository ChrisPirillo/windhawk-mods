#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdio>

struct{bool DisableWindowOpeningAnimations=true,InstallerUi=true,OpeningAnimationProcessTarget=false,OpeningAnimationAllApplications=true;int MenuCornerStyle=2;}g_settings;
static bool g_menu=false,g_mica=false,g_darkModeActive=true;
static std::atomic<bool>g_darkModeUnloading{false};
static constexpr DWORD kDwmwaBorderColor=34,kDwmwaUseHostBackdropBrush=17,kDwmsbtMainWindow=2;
static constexpr LPCWSTR kPropertyDialogDarkModeMarker=L"OwnDispatcher.PropertyDark";
struct MicaNativeFrame{bool haveBackdrop=false,haveHostBrush=false;DWORD backdrop=0;BOOL hostBrush=FALSE;};
static MicaNativeFrame micaRecord;
template<typename T>static void MicaNativeRecord(HWND,T update){update(micaRecord);}
static bool IsClassName(HWND w,LPCWSTR n){wchar_t c[100]{};return GetClassNameW(w,c,100)&&!_wcsicmp(c,n);}
static bool MenuShouldOverrideDwmFrame(HWND){return g_menu;}
static COLORREF MenuAcrylicDwmBorderColor(bool){return RGB(22,33,44);}
static bool ExplorerMicaActive(){return g_mica;}
static bool FilePickerMicaActive(){return false;}
static HWND FilePickerRoot(HWND){return nullptr;}
static bool IsCurrentProcessRegedit(){return false;}
static bool IsSystemDarkMode(){return true;}
static bool IsCustomDarkModeAllowed(){return true;}
static unsigned invalidations=0;
static void HwndDarkCacheInvalidate(){++invalidations;}
static bool DialogPresentationHoldTransitions(HWND){return false;}
namespace WindhawkUtils{using Callback=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){return reinterpret_cast<Callback>(id)(w,m,p,l,data);}static BOOL SetWindowSubclassFromAnyThread(HWND w,Callback c,DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c),d);}static BOOL RemoveWindowSubclassFromAnyThread(HWND w,Callback c){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c));}}
static unsigned nativeCalls=0,alternateCalls=0;static DWORD lastAttribute=0,lastSize=0,lastValue=0;static LPCVOID lastPointer=nullptr;static HRESULT lastResult=E_UNEXPECTED;
static HRESULT WINAPI Native(HWND w,DWORD a,LPCVOID v,DWORD n){++nativeCalls;lastAttribute=a;lastSize=n;lastPointer=v;lastValue=(v&&n>=4)?*static_cast<const DWORD*>(v):0;lastResult=DwmSetWindowAttribute(w,a,v,n);return lastResult;}
static HRESULT WINAPI Alternate(HWND w,DWORD a,LPCVOID v,DWORD n){++alternateCalls;return Native(w,a,v,n);}
static decltype(&DwmSetWindowAttribute)DwmSetWindowAttribute_orig=nullptr;
static HRESULT ConsistentCaptionDwmSet(HWND,DWORD,LPCVOID,DWORD);
static HRESULT WINAPI OpeningDwmSetAttribute_hook(HWND,DWORD,LPCVOID,DWORD);
#ifdef REJECTED
#include "rejected-binding.inc"
#include "rejected-dispatch.inc"
#include "rejected-opening.inc"
#include "rejected-caption.inc"
#else
#include "candidate-binding.inc"
#include "candidate-dispatch.inc"
#include "candidate-opening.inc"
#include "candidate-caption.inc"
#endif
using Function=decltype(&DwmSetWindowAttribute);
static Function registered=nullptr;static unsigned registrationAttempts=0,registrationRejects=0;
// Models the observed installed failure (one target accepted, second rejected).
// This does NOT pretend to execute the Windhawk engine; the actual own-DLL
// snapshot supplied by the picker agent is the evidence of the installed null.
static bool Register(Function,Function hook,Function*original){++registrationAttempts;if(registered){++registrationRejects;return false;}registered=hook;*original=Native;return true;}
static void Setup(){
#ifdef REJECTED
#include "rejected-registration.inc"
#else
#include "candidate-registration.inc"
#endif
}
static unsigned checks=0,errors=0;static void Check(bool v,const char*label){++checks;errors+=!v;printf("%s %s\n",v?"PASS":"FAIL",label);}
static bool armedBeforeCreate=false,releasedBeforeClose=false;
static LRESULT CALLBACK Root(HWND w,UINT m,WPARAM p,LPARAM l){if(m==WM_NCCREATE){auto s=OpeningTransitionFind(w);armedBeforeCreate=s&&s->owned;}if(m==WM_CLOSE){auto s=OpeningTransitionFind(w);releasedBeforeClose=s&&!s->owned;}return DefWindowProcW(w,m,p,l);}
static LRESULT CALLBACK Cbt(int c,WPARAM w,LPARAM l){if(c==HCBT_CREATEWND){auto create=reinterpret_cast<CBT_CREATEWNDW*>(l)->lpcs;ConsistentCaptionObserveNewWindow(reinterpret_cast<HWND>(w),create);OpeningTransitionObserveNewWindow(reinterpret_cast<HWND>(w),create);}return CallNextHookEx(nullptr,c,w,l);}
int main(){HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW wc{};wc.lpfnWndProc=Root;wc.lpszClassName=L"CabinetWClass";wc.hInstance=GetModuleHandleW(nullptr);RegisterClassW(&wc);Setup();Check(registrationAttempts==1&&registrationRejects==0,"one DWM target registration covers every policy");Check(DwmSetWindowAttribute_orig&&OpeningDwmSetAttribute_orig,"both policy names have an actual original trampoline");Check(&DwmSetWindowAttribute_orig==&OpeningDwmSetAttribute_orig,"both paths share the canonical live pointer storage");
HHOOK hook=SetWindowsHookExW(WH_CBT,Cbt,nullptr,GetCurrentThreadId());HWND w=CreateWindowExW(0,wc.lpszClassName,L"Own hidden DWM dispatcher",WS_OVERLAPPEDWINDOW,-30000,-30000,200,160,nullptr,nullptr,wc.hInstance,nullptr);UnhookWindowsHookEx(hook);Check(w!=nullptr,"native own window created");Check(armedBeforeCreate,"real native transition setter is armed before application creation handler");auto caption=ConsistentCaptionFind(w);auto opening=OpeningTransitionFind(w);Check(caption&&opening,"both exact production lifetime observers attached");
COLORREF authored=RGB(70,80,90);HRESULT hr=registered(w,35,&authored,sizeof(authored));Check(SUCCEEDED(hr),"registered hook forwards actual native caption setter");Check(caption&&caption->colors[0].known&&caption->colors[0].requested==authored,"caption application baseline captured through registered dispatcher");ConsistentCaptionAttribute(w,35,nullptr,nullptr,nullptr,true,RGB(32,32,32));Check(caption&&caption->colors[0].owned&&lastValue==RGB(32,32,32),"caption ownership reaches actual native setter through same trampoline");COLORREF text=RGB(210,220,230);registered(w,36,&text,sizeof(text));Check(caption&&caption->colors[1].requested==text,"text color also dispatches through application ownership tracker");ConsistentCaptionAttribute(w,36,nullptr,nullptr,nullptr,true,RGB(240,240,240));Check(caption&&caption->colors[1].owned&&lastValue==RGB(240,240,240),"text ownership actual native setter succeeds");
BOOL appEnabled=FALSE;registered(w,DWMWA_TRANSITIONS_FORCEDISABLED,&appEnabled,sizeof(appEnabled));Check(opening&&opening->owned&&!opening->requested&&lastValue==TRUE,"registered dispatcher retains opening override against app transition assignment");COLORREF newer=RGB(8,9,10);registered(w,35,&newer,sizeof(newer));unsigned before=nativeCalls;ConsistentCaptionAttribute(w,35,nullptr,nullptr,nullptr,true,RGB(32,32,32));Check(caption&&!caption->colors[0].owned&&caption->colors[0].requested==newer&&nativeCalls==before,"newer caption app assignment survives repeated apply through dispatcher");
g_menu=true;DWORD corner=DWMWCP_DONOTROUND;registered(w,DWMWA_WINDOW_CORNER_PREFERENCE,&corner,sizeof(corner));Check(lastAttribute==DWMWA_WINDOW_CORNER_PREFERENCE&&lastValue==2,"existing menu corner policy retained");COLORREF border=RGB(1,2,3);registered(w,34,&border,sizeof(border));Check(lastAttribute==34&&lastValue==RGB(22,33,44),"existing menu border policy retained");g_menu=false;
g_mica=true;DWORD backdrop=1;registered(w,DWMWA_SYSTEMBACKDROP_TYPE,&backdrop,sizeof(backdrop));Check(lastAttribute==DWMWA_SYSTEMBACKDROP_TYPE&&lastValue==2&&micaRecord.haveBackdrop&&micaRecord.backdrop==1,"existing Explorer Mica conversion and baseline tracking retained");BOOL host=TRUE;registered(w,17,&host,sizeof(host));Check(lastAttribute==DWMWA_SYSTEMBACKDROP_TYPE&&lastValue==2&&micaRecord.haveHostBrush&&micaRecord.hostBrush,"existing host-brush Mica conversion retained");g_mica=false;
SetPropW(w,kPropertyDialogDarkModeMarker,reinterpret_cast<HANDLE>(1));BOOL dark=FALSE;before=nativeCalls;unsigned previousInvalidations=invalidations;registered(w,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));Check(nativeCalls==before+2&&lastAttribute==DWMWA_USE_IMMERSIVE_DARK_MODE&&lastValue==TRUE&&invalidations==previousInvalidations+1,"existing bounded dark counter-apply and cache invalidation retained");before=nativeCalls;registered(w,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));Check(nativeCalls==before+1&&lastValue==FALSE,"existing dark counter-apply stays bounded to one reapplication");RemovePropW(w,kPropertyDialogDarkModeMarker);RemovePropW(w,L"_PropDkCounterApplied");
DWORD opaque=123;hr=registered(w,0x7FFFFFFE,&opaque,sizeof(opaque));Check(hr==lastResult&&FAILED(hr)&&lastPointer==&opaque&&lastSize==sizeof(opaque),"unrelated invalid attribute preserves native arguments and failure result");hr=registered(w,35,nullptr,0);Check(hr==lastResult&&FAILED(hr)&&lastPointer==nullptr&&lastSize==0,"invalid caption arguments preserve native failure without ownership mutation");
DwmSetWindowAttribute_orig=Alternate;Check(OpeningDwmSetAttribute_orig==Alternate,"engine original-pointer updates reach opening and caption aliases immediately");unsigned priorAlternate=alternateCalls;registered(w,36,&text,sizeof(text));Check(alternateCalls==priorAlternate+1,"caption path uses replacement canonical pointer");DwmSetWindowAttribute_orig=Native;
ConsistentCaptionsRestoreAll();Check(caption&&!caption->colors[1].owned&&lastValue==text,"owned caption text restored through sole native trampoline");SendMessageW(w,WM_CLOSE,0,0);Check(releasedBeforeClose,"opening suppression restored before application native close");Check(!OpeningTransitionFind(w)&&!ConsistentCaptionFind(w),"native destruction clears both observer lifetimes");Check(foreground==GetForegroundWindow(),"foreground unchanged; no visible window shown");printf("checks=%u failures=%u native_calls=%u; registrar is a model, DWM setters are real\n",checks,errors,nativeCalls);return errors?1:0;}
