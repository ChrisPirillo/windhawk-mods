#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdio>
struct {bool DisableWindowOpeningAnimations=true,InstallerUi=true,OpeningAnimationProcessTarget=false,OpeningAnimationAllApplications=false;}g_settings;
static bool regedit=false;
static bool IsCurrentProcessRegedit(){return regedit;}
static bool IsClassName(HWND w,LPCWSTR name){wchar_t cls[128]{};return GetClassNameW(w,cls,128)&&!_wcsicmp(cls,name);}
struct OpeningTransitionState {HWND window=nullptr;bool owned=false;};
static std::mutex g_openingTransitionsMutex;
static std::unordered_map<HWND,std::shared_ptr<OpeningTransitionState>>g_openingTransitions;
static std::atomic<bool>g_openingTransitionsUnloading{false};
static bool OpeningTransitionEnabled(){return g_settings.DisableWindowOpeningAnimations&&!g_openingTransitionsUnloading;}
static unsigned armed=0;
static void OpeningTransitionArm(const std::shared_ptr<OpeningTransitionState>&s){s->owned=true;++armed;}
static void OpeningTransitionRelease(const std::shared_ptr<OpeningTransitionState>&s){s->owned=false;}
static LRESULT CALLBACK OpeningTransitionSubclass(HWND w,UINT m,WPARAM p,LPARAM l,DWORD_PTR){return DefSubclassProc(w,m,p,l);}
namespace WindhawkUtils {
using Callback=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);
static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){return reinterpret_cast<Callback>(id)(w,m,p,l,data);}
static BOOL SetWindowSubclassFromAnyThread(HWND w,Callback c,DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c),d);}
static BOOL RemoveWindowSubclassFromAnyThread(HWND w,Callback c){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c));}
}
#ifdef REJECTED
#include "rejected.inc"
#else
#include "candidate.inc"
#endif
static unsigned errors=0,checks=0;static bool earlyArmed=false;
static void Check(bool value,const char*name){++checks;errors+=!value;printf("%s %s\n",value?"PASS":"FAIL",name);}
static LRESULT CALLBACK Root(HWND w,UINT m,WPARAM p,LPARAM l){if(m==WM_NCCREATE){auto i=g_openingTransitions.find(w);earlyArmed=i!=g_openingTransitions.end()&&i->second->owned;}return DefWindowProcW(w,m,p,l);}
static HWND Make(LPCWSTR cls,DWORD style,DWORD ex=0,HWND parent=nullptr){return CreateWindowExW(ex,cls,L"Owned hidden animation policy",style,-30000,-30000,100,100,parent,nullptr,GetModuleHandleW(nullptr),nullptr);}
static void Verify(LPCWSTR cls,DWORD style,DWORD ex,HWND parent,bool expected,const char*label){HWND w=Make(cls,style,ex,parent);Check(w!=nullptr,"native hidden window constructed");if(!w)return;CREATESTRUCTW cs{};cs.style=style;cs.dwExStyle=ex;cs.hwndParent=parent;unsigned prior=armed;OpeningTransitionObserveNewWindow(w,&cs);Check((g_openingTransitions.count(w)!=0)==expected,label);Check(armed==prior+(expected?1:0),"only eligible newly observed root is armed");Check(!IsWindowVisible(w),"policy test never shows the window");if(g_openingTransitions.count(w)){OpeningTransitionObserveNewWindow(w,&cs);Check(armed==prior+1,"duplicate observation does not arm twice");WindhawkUtils::RemoveWindowSubclassFromAnyThread(w,OpeningTransitionSubclass);g_openingTransitions.erase(w);}DestroyWindow(w);}
static LRESULT CALLBACK Cbt(int code,WPARAM w,LPARAM l){if(code==HCBT_CREATEWND)OpeningTransitionObserveNewWindow(reinterpret_cast<HWND>(w),reinterpret_cast<CBT_CREATEWNDW*>(l)->lpcs);return CallNextHookEx(nullptr,code,w,l);}
int main(){HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW wc{};wc.lpfnWndProc=Root;wc.hInstance=GetModuleHandleW(nullptr);for(LPCWSTR name:{L"OwnApplication",L"RegEdit_RegEdit",L"WixStdBA",L"TWizardForm"}){wc.lpszClassName=name;RegisterClassW(&wc);}
Verify(L"OwnApplication",WS_OVERLAPPEDWINDOW,0,nullptr,false,"existing default preserves unlisted application frames");
g_settings.OpeningAnimationAllApplications=true;
Verify(L"OwnApplication",WS_OVERLAPPEDWINDOW,0,nullptr,true,"all-applications covers ordinary unlisted native frames");
Verify(L"OwnApplication",WS_POPUP|WS_CAPTION,0,nullptr,true,"captioned owned dialog-like frames are covered");
Verify(L"OwnApplication",WS_POPUP|WS_THICKFRAME,0,nullptr,true,"frameless resizable application frame is covered");
Verify(L"OwnApplication",WS_POPUP,0,nullptr,false,"unadorned auxiliary popup remains native");
Verify(L"OwnApplication",WS_OVERLAPPEDWINDOW,WS_EX_TOOLWINDOW,nullptr,false,"tool-window transitions remain native");
Verify(L"OwnApplication",WS_OVERLAPPEDWINDOW,WS_EX_NOACTIVATE,nullptr,false,"no-activate surfaces remain native");
HWND parent=Make(L"OwnApplication",WS_OVERLAPPEDWINDOW);
Verify(L"STATIC",WS_CHILD|WS_CAPTION,0,parent,false,"child/control animation remains native");
Verify(L"OwnApplication",WS_POPUP|WS_CAPTION,0,HWND_MESSAGE,false,"message-only window is rejected");
DestroyWindow(parent);
Verify(L"WixStdBA",WS_POPUP,0,nullptr,true,"known installer coverage is preserved");
Verify(L"#32770",WS_POPUP,0,nullptr,true,"standard dialog coverage is preserved");
regedit=true;Verify(L"RegEdit_RegEdit",WS_POPUP,0,nullptr,true,"Regedit coverage is preserved");regedit=false;
g_settings.OpeningAnimationProcessTarget=true;Verify(L"OwnApplication",WS_POPUP,0,nullptr,true,"explicit process enrollment still supports its custom frames");g_settings.OpeningAnimationProcessTarget=false;
g_settings.DisableWindowOpeningAnimations=false;Verify(L"OwnApplication",WS_OVERLAPPEDWINDOW,0,nullptr,false,"master option off leaves all frames native");g_settings.DisableWindowOpeningAnimations=true;
HHOOK hook=SetWindowsHookExW(WH_CBT,Cbt,nullptr,GetCurrentThreadId());HWND w=Make(L"OwnApplication",WS_OVERLAPPEDWINDOW);UnhookWindowsHookEx(hook);Check(earlyArmed,"native CBT observation covers application frame before WM_NCCREATE");Check(!IsWindowVisible(w),"early observation never shows window");if(g_openingTransitions.count(w)){WindhawkUtils::RemoveWindowSubclassFromAnyThread(w,OpeningTransitionSubclass);g_openingTransitions.erase(w);}DestroyWindow(w);
Check(g_openingTransitions.empty(),"all fixture callback state removed");Check(foreground==GetForegroundWindow(),"foreground window unchanged");printf("checks=%u failures=%u\n",checks,errors);return errors?1:0;}
