#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <cstdio>
#include <atomic>
#include <initializer_list>
static bool explorerProcess=true;static bool IsCurrentProcessExplorer(){return explorerProcess;}
static std::atomic<bool>g_darkModeExcluded{false};static bool g_darkModeActive=true;
static bool IsClassName(HWND w,LPCWSTR n){wchar_t c[100]{};return GetClassNameW(w,c,100)&&!_wcsicmp(c,n);}
static bool NavDividerRootContainsNamespaceTree(HWND,bool){return false;}
#include "explorer.inc"
#include "desktop.inc"
#include "folder-view.inc"
static const wchar_t*kAppliedThemeProp=L"OwnThemeTest.AppliedTheme";
static unsigned calls=0;static LPCWSTR lastApp=nullptr,lastId=nullptr;static bool failNext=false;
static HRESULT WINAPI NativeTheme(HWND w,LPCWSTR app,LPCWSTR id){++calls;lastApp=app;lastId=id;if(failNext){failNext=false;return E_ACCESSDENIED;}return SetWindowTheme(w,app,id);}
static decltype(&SetWindowTheme)SetWindowTheme_orig=NativeTheme;
#include "theme-cache.inc"
#ifdef REJECTED
#include "theme-rejected.inc"
#else
#include "theme-candidate.inc"
#endif
static unsigned checks=0,errors=0;static void Check(bool v,const char*n){++checks;errors+=!v;printf("%s %s\n",v?"PASS":"FAIL",n);}
static HWND Make(LPCWSTR cls,HWND parent=nullptr){return CreateWindowExW(0,cls,L"Own hidden theme scope",parent?WS_CHILD:WS_OVERLAPPEDWINDOW,-30000,-30000,300,200,parent,nullptr,GetModuleHandleW(nullptr),nullptr);}
static void NativeContract(HWND list){for(LPCWSTR app:{static_cast<LPCWSTR>(nullptr),L"",L"Explorer",L"DarkMode_Explorer",L"ItemsView",L"AuthoredTheme"}){for(LPCWSTR id:{static_cast<LPCWSTR>(nullptr),L"",L"AuthoredId"}){SetPropW(list,kAppliedThemeProp,reinterpret_cast<HANDLE>(GetKnownThemeSignature(app,id)));unsigned before=calls;HRESULT hr=SetWindowTheme_hook(list,app,id);Check(calls==before+1&&lastApp==app&&lastId==id,"native file-list hook forwards exact pointers without cache or theme substitution");Check(SUCCEEDED(hr)&&!GetPropW(list,kAppliedThemeProp),"successful native theme clears stale mod signature");}}
SetPropW(list,kAppliedThemeProp,reinterpret_cast<HANDLE>(0x123));failNext=true;unsigned before=calls;HRESULT hr=SetWindowTheme_hook(list,L"FailingAuthoredTheme",nullptr);Check(hr==E_ACCESSDENIED&&calls==before+1,"native failure result preserved");Check(GetPropW(list,kAppliedThemeProp)==reinterpret_cast<HANDLE>(0x123),"failed native theme preserves cached signature");RemovePropW(list,kAppliedThemeProp);Check(!IsWindowVisible(list),"owned control remains hidden");}
int main(){HWND foreground=GetForegroundWindow();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_TREEVIEW_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;for(LPCWSTR name:{L"CabinetWClass",L"ExploreWClass",L"WorkerW",L"Progman",L"SHELLDLL_DefView",L"OwnUnrelatedRoot"}){wc.lpszClassName=name;RegisterClassW(&wc);}
for(LPCWSTR rootName:{L"CabinetWClass",L"ExploreWClass",L"WorkerW",L"Progman"}){HWND root=Make(rootName),view=Make(L"SHELLDLL_DefView",root),list=Make(WC_LISTVIEWW,view);Check(root&&view&&list,"native hierarchy constructed");NativeContract(list);DestroyWindow(root);}
// Excluded scopes retain the previous native/mod dispatch, rather than
// broadening a root-class check to unrelated descendants or owned dialogs.
for(LPCWSTR rootName:{L"#32770",L"OwnUnrelatedRoot"}){HWND root=Make(rootName),view=Make(L"SHELLDLL_DefView",root),list=Make(WC_LISTVIEWW,view);RemovePropW(list,kAppliedThemeProp);unsigned before=calls;SetWindowTheme_hook(list,L"AuthoredTheme",L"AuthoredId");Check(calls==before+1&&lastApp&&_wcsicmp(lastApp,L"DarkMode_Explorer")==0,"Properties/picker/unrelated list retains existing Explorer-family behavior");DestroyWindow(root);}
HWND root=Make(L"CabinetWClass"),tree=Make(WC_TREEVIEWW,root),list=Make(WC_LISTVIEWW,root);SetWindowTheme_hook(tree,L"AuthoredTheme",L"AuthoredId");Check(lastApp&&_wcsicmp(lastApp,L"DarkMode_Explorer")==0,"navigation tree retains existing theme behavior");SetWindowTheme_hook(list,L"AuthoredTheme",L"AuthoredId");Check(lastApp&&_wcsicmp(lastApp,L"DarkMode_Explorer")==0,"unrelated Cabinet child list without shell DefView retains behavior");DestroyWindow(root);
root=Make(L"CabinetWClass");HWND dialog=CreateWindowExW(0,L"#32770",L"Own owned Properties",WS_OVERLAPPEDWINDOW,-30000,-30000,300,200,root,nullptr,GetModuleHandleW(nullptr),nullptr);HWND view=Make(L"SHELLDLL_DefView",dialog);list=Make(WC_LISTVIEWW,view);SetWindowTheme_hook(list,L"AuthoredTheme",L"AuthoredId");Check(lastApp&&_wcsicmp(lastApp,L"DarkMode_Explorer")==0,"Properties owned by Explorer is distinguished by GA_ROOT");DestroyWindow(dialog);DestroyWindow(root);
explorerProcess=false;root=Make(L"CabinetWClass");view=Make(L"SHELLDLL_DefView",root);list=Make(WC_LISTVIEWW,view);unsigned before=calls;SetWindowTheme_hook(list,L"AuthoredTheme",L"AuthoredId");Check(calls==before+1&&lastApp&&_wcsicmp(lastApp,L"AuthoredTheme")==0,"other process is unaffected by native Explorer-only guard");DestroyWindow(root);
Check(foreground==GetForegroundWindow(),"foreground unchanged");printf("checks=%u failures=%u; native API call contracts, no external Explorer rendering certification\n",checks,errors);return errors?1:0;}
