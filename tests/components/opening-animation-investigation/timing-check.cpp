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
struct{bool DisableWindowOpeningAnimations=true,InstallerUi=false,OpeningAnimationProcessTarget=false,OpeningAnimationAllApplications=true;}g_settings;
static bool IsClassName(HWND w,LPCWSTR n){wchar_t c[100]{};return GetClassNameW(w,c,100)&&!_wcsicmp(c,n);}
static bool IsCurrentProcessRegedit(){return false;}
static bool DialogPresentationHoldTransitions(HWND){return false;}
namespace WindhawkUtils{using Callback=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){return reinterpret_cast<Callback>(id)(w,m,p,l,data);}static BOOL SetWindowSubclassFromAnyThread(HWND w,Callback c,DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c),d);}static BOOL RemoveWindowSubclassFromAnyThread(HWND w,Callback c){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c));}}
#ifdef REJECTED
#include "timing-rejected.inc"
#else
#include "timing-candidate.inc"
#endif
static bool armedAtNcCreate=false,armedAtCreate=false,releasedAtClose=false;static unsigned failedDwm=0,successDwm=0;static BOOL assigned=FALSE;
static HRESULT WINAPI TraceDwm(HWND w,DWORD a,LPCVOID value,DWORD size){HRESULT hr=DwmSetWindowAttribute(w,a,value,size);if(FAILED(hr))++failedDwm;else{++successDwm;if(a==DWMWA_TRANSITIONS_FORCEDISABLED)assigned=*static_cast<const BOOL*>(value);}return hr;}
static LRESULT CALLBACK Root(HWND w,UINT m,WPARAM p,LPARAM l){auto s=OpeningTransitionFind(w);if(m==WM_NCCREATE)armedAtNcCreate=s&&s->owned;if(m==WM_CREATE)armedAtCreate=s&&s->owned;if(m==WM_CLOSE)releasedAtClose=s&&!s->owned&&!assigned;return DefWindowProcW(w,m,p,l);}
static LRESULT CALLBACK Cbt(int c,WPARAM w,LPARAM l){if(c==HCBT_CREATEWND)OpeningTransitionObserveNewWindow(reinterpret_cast<HWND>(w),reinterpret_cast<CBT_CREATEWNDW*>(l)->lpcs);return CallNextHookEx(nullptr,c,w,l);}
int main(){unsigned errors=0,checks=0;auto check=[&](bool v,const char*n){++checks;errors+=!v;printf("%s %s\n",v?"PASS":"FAIL",n);};HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW wc{};wc.lpfnWndProc=Root;wc.lpszClassName=L"OwnOpeningNativeTiming";wc.hInstance=GetModuleHandleW(nullptr);RegisterClassW(&wc);OpeningDwmSetAttribute_orig=TraceDwm;
HHOOK h=SetWindowsHookExW(WH_CBT,Cbt,nullptr,GetCurrentThreadId());HWND w=CreateWindowExW(0,wc.lpszClassName,L"Own native hidden timing",WS_OVERLAPPEDWINDOW,-30000,-30000,100,100,nullptr,nullptr,wc.hInstance,nullptr);UnhookWindowsHookEx(h);check(w!=nullptr,"native root created");check(failedDwm>=1,"real DWM rejects the premature HCBT handle on this host");check(armedAtNcCreate,"real DWM suppression armed before native WM_NCCREATE handler");check(armedAtCreate,"real DWM suppression armed before native WM_CREATE handler");check(successDwm>=1,"real DWM accepts the retried native attribute");check(!IsWindowVisible(w),"no window shown");auto s=OpeningTransitionFind(w);check(s&&s->owned,"window remains armed until a show or close boundary");BOOL off=FALSE;OpeningDwmSetAttribute_hook(w,DWMWA_TRANSITIONS_FORCEDISABLED,&off,sizeof(off));check(s&&!s->requested&&assigned,"application preference retained behind opening override");SendMessageW(w,WM_CLOSE,0,0);check(releasedAtClose,"native closing preference restored before application close handler");check(!OpeningTransitionFind(w),"native teardown removes state");check(foreground==GetForegroundWindow(),"foreground unchanged");printf("checks=%u failures=%u; real DWM calls, no visible animation observation\n",checks,errors);return errors?1:0;}
