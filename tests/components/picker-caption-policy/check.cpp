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
struct Write{HWND window;DWORD attribute;COLORREF color;};
static std::vector<Write>writes;static bool failWrites=false;
static HRESULT WINAPI NativeSetter(HWND w,DWORD a,LPCVOID p,DWORD n){if(failWrites)return E_FAIL;HRESULT hr=DwmSetWindowAttribute(w,a,p,n);if(SUCCEEDED(hr)&&p&&n==sizeof(COLORREF))writes.push_back({w,a,*static_cast<const COLORREF*>(p)});return hr;}
static decltype(&DwmSetWindowAttribute)OpeningDwmSetAttribute_orig=NativeSetter;
namespace WindhawkUtils{using Callback=LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR);static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){return reinterpret_cast<Callback>(id)(w,m,p,l,data);}static BOOL SetWindowSubclassFromAnyThread(HWND w,Callback c,DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c),d);}static BOOL RemoveWindowSubclassFromAnyThread(HWND w,Callback c){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(c));}}
#include "caption-owner.inc"
static bool g_darkModeActive=true,g_custom=true,g_mica=false;
static std::atomic<bool>g_darkModeUnloading{false};
namespace WinUI{namespace Dark{static constexpr COLORREF TextFillColorPrimary=RGB(255,255,255);}}
static bool IsCustomDarkModeAllowed(){return g_custom;}
static COLORREF FilePickerControlBackground(bool){return g_mica?RGB(44,44,44):RGB(43,43,43);}
static bool IsClassName(HWND w,LPCWSTR name){wchar_t c[128]{};return w&&GetClassNameW(w,c,128)&&!_wcsicmp(c,name);}
static BOOL CALLBACK FindTree(HWND w,LPARAM out){if(IsClassName(w,L"NamespaceTreeControl"))*reinterpret_cast<bool*>(out)=true;return TRUE;}
static HWND FilePickerDialogRoot(HWND w){HWND root=GetAncestor(w,GA_ROOT);bool tree=false;if(IsClassName(root,L"#32770")&&FindWindowExW(root,nullptr,L"DUIViewWndClassName",nullptr))EnumChildWindows(root,FindTree,reinterpret_cast<LPARAM>(&tree));return tree?root:nullptr;}
#include "picker-root.inc"
#include "picker-policy.inc"
static LRESULT CALLBACK Cbt(int c,WPARAM w,LPARAM l){if(c==HCBT_CREATEWND)ConsistentCaptionObserveNewWindow(reinterpret_cast<HWND>(w),reinterpret_cast<CBT_CREATEWNDW*>(l)->lpcs);return CallNextHookEx(nullptr,c,w,l);}
static HWND Make(bool observed=true,LPCWSTR name=L"#32770"){HHOOK hook=observed?SetWindowsHookExW(WH_CBT,Cbt,nullptr,GetCurrentThreadId()):nullptr;HWND w=CreateWindowExW(0,name,L"Own hidden picker caption policy",WS_OVERLAPPEDWINDOW,-30000,-30000,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(hook)UnhookWindowsHookEx(hook);return w;}
static HWND Child(HWND root,LPCWSTR name,int id=0){return CreateWindowExW(0,name,L"",WS_CHILD,0,0,20,20,root,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);}
static void Modern(HWND w){HWND host=Child(w,L"DUIViewWndClassName");Child(host,L"NamespaceTreeControl");}
static void Classic(HWND w){Child(w,L"SHELLDLL_DefView",1121);Child(w,L"ComboBox",1136);Child(w,L"ComboBox",1137);}
static void Generic(HWND w,DWORD a,bool on,COLORREF c=RGB(32,32,32)){ConsistentCaptionAttribute(w,a,nullptr,nullptr,nullptr,on,c);}
int main(){
 unsigned errors=0,checks=0;auto check=[&](bool okay,const char* name){checks++;errors+=!okay;printf("%s %s\n",okay?"PASS":"FAIL",name);};
 HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 for(LPCWSTR name:{L"DUIViewWndClassName",L"NamespaceTreeControl",L"SHELLDLL_DefView",L"CabinetWClass"}){WNDCLASSW c{};c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=name;RegisterClassW(&c);}
 HWND plain=Make();Generic(plain,35,true);Generic(plain,36,true,RGB(220,220,220));auto state=ConsistentCaptionFind(plain);size_t before=writes.size();FilePickerCaptionRefresh(plain,WM_PAINT);
 check(writes.size()==before&&state->colors[0].caller==&kConsistentCaptionGenericCaller&&state->colors[0].owned,"ineligible Properties paint cannot release or change generic caption");
 FilePickerCaptionRefresh(plain,WM_NCDESTROY);check(writes.size()==before&&state->colors[1].owned,"ineligible destroy trigger cannot clean generic text owner");DestroyWindow(plain);
 HWND modern=Make();HWND host=Child(modern,L"DUIViewWndClassName");FilePickerCaptionRefresh(modern,WM_SHOWWINDOW);state=ConsistentCaptionFind(modern);check(!state->colors[0].owned&&!state->colors[1].owned,"late-built picker identity not guessed before namespace tree");Child(host,L"NamespaceTreeControl");FilePickerCaptionRefresh(modern,WM_SHOWWINDOW);
 check(state->colors[0].owned&&state->colors[0].assigned==RGB(43,43,43)&&state->colors[0].caller==&kFilePickerCaptionCaller,"modern picker title uses exact footer color and independent caller");
 check(state->colors[1].owned&&state->colors[1].assigned==WinUI::Dark::TextFillColorPrimary,"modern picker text has same owner's independent native assignment");
 before=writes.size();FilePickerCaptionRefresh(modern,WM_PAINT);check(writes.size()==before,"repeated paint does not repeat native DWM setters");
 Generic(modern,35,true,RGB(43,43,43));Generic(modern,35,false);Generic(modern,36,false);check(writes.size()==before&&state->colors[0].owned&&state->colors[1].owned,"generic equal-color claim and cleanup cannot touch picker-owned slots");
 g_mica=true;FilePickerCaptionRefresh(modern,WM_THEMECHANGED);check(writes.size()==before+1&&state->colors[0].assigned==RGB(44,44,44),"Mica footer palette change updates only matching picker title");g_mica=false;
 COLORREF newer=RGB(71,72,73);ConsistentCaptionDwmSet(modern,35,&newer,sizeof(newer));before=writes.size();FilePickerCaptionRefresh(modern,WM_PAINT);Generic(modern,35,false);FilePickerCaptionRefresh(modern,WM_PAINT);
 check(writes.size()==before&&!state->colors[0].owned&&state->colors[0].superseded&&state->colors[0].caller==&kFilePickerCaptionCaller,"later application title wins repeated policy and foreign cleanup");
 g_darkModeActive=false;FilePickerCaptionRefresh(modern,WM_SETTINGCHANGE);check(!state->colors[0].superseded&&!state->colors[0].caller&&!state->colors[1].owned&&state->colors[0].requested==newer,"own explicit disable clears its cycle and preserves application title baseline");
 check(writes.back().attribute==36&&writes.back().color==0xFFFFFFFF,"disable restores only text still actually owned");g_darkModeActive=true;
 FilePickerCaptionRefresh(modern,WM_PAINT);g_custom=false;FilePickerCaptionRefresh(modern,WM_THEMECHANGED);check(!state->colors[0].owned&&!state->colors[1].owned,"high-contrast/exclusion gate adapter restores own caption colors");before=writes.size();FilePickerCaptionRefresh(modern,WM_PAINT);check(writes.size()==before,"disallowed custom dark mode does not reacquire");g_custom=true;
 HWND overlap=Make();Modern(overlap);Generic(overlap,35,true);auto other=ConsistentCaptionFind(overlap);FilePickerCaptionRefresh(overlap,WM_PAINT);
 check(other->colors[0].assigned==RGB(32,32,32)&&other->colors[0].caller==&kConsistentCaptionGenericCaller,"picker refuses a background already owned by generic adapter");
 check(other->colors[1].caller==&kFilePickerCaptionCaller&&other->colors[1].owned,"independent unclaimed text slot can join picker policy");
 g_darkModeActive=false;FilePickerCaptionRefresh(overlap,WM_SETTINGCHANGE);check(other->colors[0].owned&&!other->colors[1].owned,"picker release preserves foreign background owner");g_darkModeActive=true;Generic(overlap,35,false);FilePickerCaptionRefresh(overlap,WM_PAINT);check(other->colors[0].caller==&kFilePickerCaptionCaller,"explicit foreign release permits a subsequent picker cycle");
 HWND textOverlap=Make();Modern(textOverlap);Generic(textOverlap,36,true,RGB(202,203,204));FilePickerCaptionRefresh(textOverlap,WM_PAINT);auto ts=ConsistentCaptionFind(textOverlap);g_darkModeActive=false;FilePickerCaptionRefresh(textOverlap,WM_SETTINGCHANGE);check(ts->colors[1].owned&&ts->colors[1].assigned==RGB(202,203,204)&&!ts->colors[0].owned,"picker release preserves independently foreign text owner");g_darkModeActive=true;
 HWND classic=Make();Classic(classic);FilePickerCaptionRefresh(classic,WM_SHOWWINDOW);auto cs=ConsistentCaptionFind(classic);check(cs->colors[0].owned&&cs->colors[0].caller==&kFilePickerCaptionCaller,"classic native control IDs enroll classic Open/Save root");
 failWrites=true;g_darkModeActive=false;FilePickerCaptionRefresh(classic,WM_SETTINGCHANGE);check(cs->colors[0].owned&&cs->colors[0].caller==&kFilePickerCaptionCaller,"failed native restoration keeps correct policy owner for retry");failWrites=false;before=writes.size();Generic(classic,35,false);check(writes.size()==before&&cs->colors[0].owned,"foreign cleanup cannot steal a failed pending restoration");FilePickerCaptionRefresh(classic,WM_SETTINGCHANGE);check(!cs->colors[0].owned&&!cs->colors[0].caller,"own restoration retries and releases caller");g_darkModeActive=true;
 HWND old=Make(false);Modern(old);before=writes.size();FilePickerCaptionRefresh(old,WM_PAINT);check(writes.size()==before&&!ConsistentCaptionFind(old),"already-open unobserved picker baseline is never guessed");COLORREF authored=RGB(18,28,38);ConsistentCaptionDwmSet(old,35,&authored,sizeof(authored));FilePickerCaptionRefresh(old,WM_PAINT);auto os=ConsistentCaptionFind(old);check(os->colors[0].owned&&!os->colors[1].owned,"observed single-attribute setter permits only that known picker color");FilePickerCaptionRefresh(old,WM_NCDESTROY);check(!os->colors[0].owned&&writes.back().color==authored,"own cleanup restores exact known application baseline");
 HWND explorer=Make(true,L"CabinetWClass");Modern(explorer);Generic(explorer,35,true,RGB(24,25,26));before=writes.size();FilePickerCaptionRefresh(explorer,WM_PAINT);check(writes.size()==before&&ConsistentCaptionFind(explorer)->colors[0].owned,"Explorer root never enters picker palette or cleanup");
 before=writes.size();FilePickerCaptionRefresh(modern,WM_MOUSEMOVE);check(writes.size()==before,"unrelated messages do not trigger policy work");
 auto dead=cs;DestroyWindow(classic);check(dead->retired&&!ConsistentCaptionFind(classic),"native NCDESTROY removes whole lifetime including policy caller");
 FilePickerCaptionRefresh(modern,WM_PAINT);ConsistentCaptionsRestoreAll();bool cleaned=true;for(auto s:{state,other,ts,os})for(auto&slot:s->colors)cleaned=cleaned&&!slot.owned&&!slot.caller;
 check(cleaned&&g_consistentCaptions.empty(),"global unload force-restores both caller families and clears identities");
 for(HWND w:{modern,overlap,textOverlap,old,explorer}){check(!IsWindowVisible(w),"native caption test window remained hidden");DestroyWindow(w);}
 check(GetForegroundWindow()==foreground,"no shown window or foreground change");printf("checks=%u failures=%u successful_native_setters=%zu; policy/ownership not painted-title acceptance\n",checks,errors,writes.size());return errors?1:0;
}
