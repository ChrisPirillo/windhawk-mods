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
#include <thread>
#include <cstdio>
struct {bool DisableWindowOpeningAnimations=false,InstallerUi=false,OpeningAnimationProcessTarget=false,OpeningAnimationAllApplications=true,ModernContextMenus=false,RoundedButtons=true,EnableDarkMode=false,RegeditSection=false;} g_settings;
static bool g_isXamlProcess=true;
static unsigned captions=0,staging=0,menus=0,widgets=0;
struct CheckAnim {int fromState;float progress;DWORD lastTick;};
static std::unordered_map<ULONG_PTR,CheckAnim>g_checkAnims;static std::recursive_mutex g_checkAnimsMutex;static std::atomic<bool>g_checkBoxAnimUnloading{false};
static LRESULT CALLBACK CheckBoxSubclassProc(HWND w,UINT m,WPARAM p,LPARAM l,DWORD_PTR){++widgets;return DefSubclassProc(w,m,p,l);}
#define MENUPOPUP_CLASS L"#32768"
static void MenuTryInstallAcrylic(HWND){++menus;}
static bool IsClassName(HWND w,LPCWSTR n){wchar_t c[100]{};return GetClassNameW(w,c,100)&&!_wcsicmp(c,n);}
static bool IsCurrentProcessRegedit(){return false;}
static bool DialogPresentationHoldTransitions(HWND){return false;}
namespace WindhawkUtils{using Callback=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){return reinterpret_cast<Callback>(id)(w,m,p,l,data);}static BOOL SetWindowSubclassFromAnyThread(HWND w,Callback c,DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c),d);}static BOOL RemoveWindowSubclassFromAnyThread(HWND w,Callback c){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c));}}
static std::unordered_map<HWND,BOOL> actualAssignments;
static unsigned nativeDwmSuccess=0;
static HRESULT WINAPI TraceDwm(HWND w,DWORD a,LPCVOID v,DWORD size){HRESULT hr=DwmSetWindowAttribute(w,a,v,size);if(SUCCEEDED(hr)&&a==DWMWA_TRANSITIONS_FORCEDISABLED&&v&&size==sizeof(BOOL)){actualAssignments[w]=*static_cast<const BOOL*>(v);++nativeDwmSuccess;}return hr;}
static decltype(&DwmSetWindowAttribute) DwmSetWindowAttribute_orig=TraceDwm;
#ifdef REJECTED
#include "rejected-cbt.inc"
#else
#include "cbt.inc"
#endif
static void ConsistentCaptionObserveNewWindow(HWND,const CREATESTRUCTW*){++captions;}
static HRESULT ConsistentCaptionDwmSet(HWND w,DWORD a,LPCVOID v,DWORD s){return DwmSetWindowAttribute_orig(w,a,v,s);}
static void DialogPresentationObserveNewWindow(HWND,const CREATESTRUCTW*){++staging;}
#include "opening.inc"
static void InstallCbtHookForThread(DWORD tid=0);
#include "installer.inc"
#ifdef REJECTED
#include "rejected-minimal.inc"
#else
#include "minimal.inc"
#endif
#include "wrappers.inc"
static unsigned checks=0,errors=0;
static void check(bool ok,const char* n){++checks;errors+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",n);}
static bool atCreate=false,atClose=false;static HWND current=nullptr;static bool makeNative=false,failNative=false,exactArgs=false;static DWORD incoming=0;
static LRESULT CALLBACK Root(HWND w,UINT m,WPARAM p,LPARAM l){auto state=OpeningTransitionFind(w);if(m==WM_NCCREATE){atCreate=state&&state->owned&&actualAssignments[w];}if(m==WM_CLOSE){atClose=state&&!state->owned&&!actualAssignments[w];}return DefWindowProcW(w,m,p,l);}
static HWND WINAPI NativeCreate(DWORD e,VOID*c,LPCWSTR name,VOID*title,DWORD style,LONG x,LONG y,LONG width,LONG height,HWND parent,HMENU menu,HINSTANCE inst,LPVOID param,DWORD show,DWORD f1,DWORD f2,VOID*q){incoming=GetLastError();exactArgs=e==1&&c==(void*)2&&name==(LPCWSTR)3&&title==(void*)4&&style==5&&x==-6&&y==7&&width==8&&height==9&&parent==(HWND)10&&menu==(HMENU)11&&inst==(HINSTANCE)12&&param==(void*)13&&show==14&&f1==15&&f2==16&&q==(void*)17;HWND result=makeNative?CreateWindowExW(0,L"OwnXamlNativeFrame",L"Never shown",WS_OVERLAPPEDWINDOW,-30000,-30000,160,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr):(failNative?nullptr:(HWND)0x1234);SetLastError(failNative?ERROR_ACCESS_DENIED:0x6789);return result;}
static HWND invoke(){return OpeningNtUserCreateWindowEx_hook(1,(void*)2,(LPCWSTR)3,(void*)4,5,-6,7,8,9,(HWND)10,(HMENU)11,(HINSTANCE)12,(void*)13,14,15,16,(void*)17);}
static DWORD showIncoming=0;static int gotCommand=0;static HWND gotWindow=nullptr,gotAfter=nullptr;static UINT gotFlags=0;static DWORD gotDuration=0;static bool releaseSeen=false;
static BOOL WINAPI ShowSpy(HWND w,int c){showIncoming=GetLastError();gotWindow=w;gotCommand=c;SetLastError(0x4567);return FALSE;}
static BOOL WINAPI PosSpy(HWND w,HWND a,int x,int y,int cx,int cy,UINT flags){showIncoming=GetLastError();gotWindow=w;gotAfter=a;gotFlags=flags;exactArgs=x==-2&&y==3&&cx==4&&cy==5;SetLastError(0x4567);return FALSE;}
static BOOL WINAPI AnimateSpy(HWND w,DWORD duration,DWORD flags){showIncoming=GetLastError();gotWindow=w;gotDuration=duration;gotFlags=flags;auto state=OpeningTransitionFind(w);releaseSeen=!state||!state->owned;SetLastError(0x4567);return FALSE;}
int main(){HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW wc{};wc.lpfnWndProc=Root;wc.lpszClassName=L"OwnXamlNativeFrame";wc.hInstance=GetModuleHandleW(nullptr);RegisterClassW(&wc);NtUserCreateWindowEx_orig=NativeCreate;
SetLastError(0xABCD);auto r=invoke();check(r==(HWND)0x1234&&exactArgs,"minimal hook forwards all17 exact arguments and return");check(incoming==0xABCD&&GetLastError()==0x6789,"master-off native last-error semantics preserved");check(g_cbtHooks.empty(),"master-off does not install observer even with rounded option set");failNative=true;SetLastError(0xABCD);r=invoke();check(!r&&exactArgs&&incoming==0xABCD&&GetLastError()==ERROR_ACCESS_DENIED,"native failed creation forwarded exactly");failNative=false;
g_settings.DisableWindowOpeningAnimations=true;makeNative=true;SetLastError(0xABCD);current=invoke();check(current&&exactArgs&&incoming==0xABCD&&GetLastError()==0x6789,"off-to-on creation keeps native arguments and last-error");check(atCreate,"real DWM flag armed before native NCCREATE on XAML-model branch");check(!g_cbtHooks.empty()&&captions>0,"menus-off opening-on installs actual per-thread CBT observer");check(staging==0&&widgets==0&&menus==0&&g_checkAnims.empty(),"XAML observer avoids dialog staging menus and checkbox pipeline");HWND button=CreateWindowExW(0,L"Button",L"Untouched",WS_CHILD|BS_AUTOCHECKBOX,0,0,60,20,current,nullptr,wc.hInstance,nullptr);check(button&&(GetWindowLongPtrW(button,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX&&g_checkAnims.empty()&&staging==0,"real child checkbox keeps native style and no animation subclass");check(!IsWindowVisible(current),"native frame never shown");SendMessageW(current,WM_CLOSE,0,0);check(atClose,"real DWM prior transition preference restored before native close");check(!OpeningTransitionFind(current),"native destruction removes opening state");
bool threadOwned=false,threadHidden=false;std::thread t([&]{HWND w=invoke();auto state=OpeningTransitionFind(w);threadOwned=state&&state->owned&&actualAssignments[w];threadHidden=w&&!IsWindowVisible(w);DestroyWindow(w);});t.join();check(threadOwned&&threadHidden,"future UI thread gets actual creation observer with menus disabled");check(staging==0&&widgets==0&&menus==0,"new UI thread does not enter full widget pipeline");
OpeningShowWindow_orig=ShowSpy;OpeningShowWindowAsync_orig=ShowSpy;OpeningSetWindowPos_orig=PosSpy;OpeningAnimateWindow_orig=AnimateSpy;HWND missing=(HWND)0x9876;
SetLastError(0x1111);check(!OpeningShowWindow_hook(missing,SW_SHOWNOACTIVATE)&&gotWindow==missing&&gotCommand==SW_SHOWNOACTIVATE&&showIncoming==0x1111&&GetLastError()==0x4567,"ShowWindow native args return and failure preserved without state");SetLastError(0x2222);check(!OpeningShowWindowAsync_hook(missing,SW_HIDE)&&gotWindow==missing&&gotCommand==SW_HIDE&&showIncoming==0x2222&&GetLastError()==0x4567,"ShowWindowAsync native hide preserved");SetLastError(0x3333);check(!OpeningSetWindowPos_hook(missing,(HWND)18,-2,3,4,5,SWP_HIDEWINDOW|SWP_NOACTIVATE)&&exactArgs&&gotWindow==missing&&gotAfter==(HWND)18&&gotFlags==(SWP_HIDEWINDOW|SWP_NOACTIVATE)&&showIncoming==0x3333&&GetLastError()==0x4567,"SetWindowPos exact args hide failure preserved");SetLastError(0x4444);check(!OpeningAnimateWindow_hook(missing,129,AW_HIDE|AW_BLEND)&&gotWindow==missing&&gotDuration==129&&gotFlags==(AW_HIDE|AW_BLEND)&&showIncoming==0x4444&&GetLastError()==0x4567&&releaseSeen,"closing AnimateWindow duration flags and failure preserved");SetLastError(0x5555);check(!OpeningAnimateWindow_hook(missing,123,AW_BLEND)&&gotDuration==123&&gotFlags==AW_BLEND&&showIncoming==0x5555&&GetLastError()==0x4567,"untracked opening animation native failure preserved");
HWND live=invoke();SetLastError(0x6666);check(!OpeningAnimateWindow_hook(live,77,AW_HIDE|AW_HOR_NEGATIVE)&&gotDuration==77&&gotFlags==(AW_HIDE|AW_HOR_NEGATIVE)&&releaseSeen&&showIncoming==0x6666&&GetLastError()==0x4567,"tracked closing releases override before exact native animation call");DestroyWindow(live);g_settings.DisableWindowOpeningAnimations=false;g_settings.EnableDarkMode=true;const unsigned priorCaptions=captions;bool captionOnly=false;std::thread captionThread([&]{HWND w=invoke();captionOnly=w&&!OpeningTransitionFind(w)&&!IsWindowVisible(w);DestroyWindow(w);});captionThread.join();check(captionOnly&&captions>priorCaptions&&staging==0&&widgets==0,"opening-off dark-on records fresh metadata on future XAML UI thread without widget pipeline");check(nativeDwmSuccess>0,"real native DWM setter successes recorded without unsupported attribute readback");check(foreground==GetForegroundWindow(),"foreground unchanged");for(auto&[tid,h]:g_cbtHooks)UnhookWindowsHookEx(h.hook);printf("checks=%u failures=%u; genuine hidden HWND CBT DWM; modeled XAML flag and native-create adapter, no compositor claim\n",checks,errors);return errors?1:0;}
