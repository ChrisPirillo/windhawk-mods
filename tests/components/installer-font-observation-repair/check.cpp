#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
struct Settings{bool GeneralSection=true,ConsistentUIFonts=true,InstallerUi=true,InstallerUiProcessTarget=false;};
static Settings g_settings;
static const Settings* GetSettings(){return &g_settings;}
static std::atomic<bool> g_roundedSelectionHighContrast=false;
static thread_local bool t_nativePropertyFontUpdate=false;
static bool IsTextPipelineDisabled(){return false;}
static bool IsCurrentProcessRegedit(){return false;}
static bool IsClassName(HWND w,LPCWSTR expected){wchar_t name[80]{};return GetClassNameW(w,name,80)&&!_wcsicmp(name,expected);}
static int CALLBACK FoundFont(const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM output){*reinterpret_cast<bool*>(output)=true;return 0;}
static bool FontFamilyExists(LPCWSTR name){
    HDC dc=GetDC(nullptr);if(!dc)return false;LOGFONTW font{};font.lfCharSet=DEFAULT_CHARSET;wcsncpy_s(font.lfFaceName,name,_TRUNCATE);
    bool found=false;EnumFontFamiliesExW(dc,&font,FoundFont,reinterpret_cast<LPARAM>(&found),0);ReleaseDC(nullptr,dc);return found;
}
namespace WindhawkUtils{
static LRESULT CALLBACK Bridge(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR d){return reinterpret_cast<LRESULT(CALLBACK*)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR)>(id)(w,m,a,b,d);}
static bool SetWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* f)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR),DWORD_PTR d){return SetWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(f),d);}
static bool RemoveWindowSubclassFromAnyThread(HWND w,LRESULT(CALLBACK* f)(HWND,UINT,WPARAM,LPARAM,DWORD_PTR)){return RemoveWindowSubclass(w,Bridge,reinterpret_cast<UINT_PTR>(f));}
}
#include "interactive.inc"
#include "engine.inc"
#include "native-font.inc"
// This fixture uses no property-tab role layer; ordinary native controls are
// exercised with the real production observer, resolver, subclass and teardown.
static void RestoreNativePropertyTabFont(HWND){}
static HFONT NativePropertyTabUnderlyingFont(HWND,HFONT font){return font;}
static HFONT Current(HWND control){return reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0));}
static bool Family(HWND control,LPCWSTR expected){LOGFONTW font{};return GetObjectW(Current(control),sizeof(font),&font)==sizeof(font)&&!_wcsicmp(font.lfFaceName,expected);}
static bool Alias(LPCWSTR base,LPCWSTR name){WNDCLASSW type{};if(!GetClassInfoW(nullptr,base,&type))return false;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=name;return RegisterClassW(&type)!=0;}
int main(){
    unsigned failures=0;auto check=[&](bool pass,const char* label){printf("%s %s\n",pass?"PASS":"FAIL",label);failures+=!pass;};
    HWND foreground=GetForegroundWindow();HINSTANCE instance=GetModuleHandleW(nullptr);
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES|ICC_TAB_CLASSES};InitCommonControlsEx(&cc);
    for(const auto* name:{L"WixStdBA",L"TWizardForm",L"TUninstallProgressForm",L"CabinetWClass",L"UnknownApplication"}){
        WNDCLASSW type{};type.hInstance=instance;type.lpfnWndProc=DefWindowProcW;type.lpszClassName=name;if(!RegisterClassW(&type))return 2;
    }
    if(!Alias(L"STATIC",L"TNewStaticText"))return 2;
    g_nativeUiFontSyncMessage=RegisterWindowMessageW(L"Acceptance.Installer.NativeFont.Sync.0ed59fab");
    const wchar_t* target=FontFamilyExists(L"Segoe UI Variable Text")?L"Segoe UI Variable Text":L"Segoe UI";
    check(IsCurrentProcessInteractive(),"actual fixture window station permits the production interactive installer scope");
    for(const auto* engine:{L"WixStdBA",L"TWizardForm",L"TUninstallProgressForm"}){
        printf("engine=%ls\n",engine);
        HWND root=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,engine,L"Own hidden font test",WS_POPUP,-30000,-30000,200,100,nullptr,nullptr,instance,nullptr);
        const wchar_t* name=!wcscmp(engine,L"WixStdBA")?L"STATIC":L"TNewStaticText";
        HWND control=CreateWindowExW(0,name,L"Native caption g j p",WS_CHILD,0,0,180,24,root,nullptr,instance,nullptr);
        if(!root||!control)return 2;
        HFONT original=CreateFontW(-16,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,DEFAULT_PITCH,L"Tahoma");
        HFONT late=CreateFontW(-18,0,0,0,700,0,0,0,DEFAULT_CHARSET,0,0,0,DEFAULT_PITCH,L"Arial");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(original),FALSE);
        check(InstallerUiRoot(control)&&NativeUiFontScope(control),"known installer scope works without process-name enrollment");
        NativeUiFontObserve(control,name);
        check(GetPropW(control,kNativeUiFontObserved)&&Family(control,target),"known non-dialog installer control receives the consistent native font");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(late),FALSE);
        LOGFONTW description{};GetObjectW(Current(control),sizeof(description),&description);
        check(Family(control,target)&&description.lfHeight==-18&&description.lfWeight==700&&NativeUiFontOriginal(control)==late,"late installer font assignments keep size/weight and record the application font");
        g_settings.InstallerUi=false;NativeUiFontRefresh();
        check(Current(control)==late,"turning the installer profile off restores the same-window application font without waiting for paint");
        g_settings.InstallerUi=true;NativeUiFontRefresh();
        check(Family(control,target),"reenabling the profile reapplies consistent typography to the existing native control");
        g_settings.ConsistentUIFonts=false;NativeUiFontRefresh();
        check(Current(control)==late&&!GetPropW(control,kNativeUiFontObserved),"global font switch restores and detaches the existing control immediately");
        g_settings.ConsistentUIFonts=true;NativeUiFontRefresh();
        check(Family(control,target)&&GetPropW(control,kNativeUiFontObserved),"global font switch can observe and normalize the existing control again");
        g_settings.GeneralSection=false;NativeUiFontRefresh();
        check(Current(control)==late&&!GetPropW(control,kNativeUiFontObserved),"General category off restores native font and removes observation");
        g_settings.GeneralSection=true;NativeUiFontRefresh();
        check(Family(control,target),"General category on reapplies the existing control font");
        HFONT symbol=CreateFontW(-16,0,0,0,400,0,0,0,SYMBOL_CHARSET,0,0,0,DEFAULT_PITCH,L"Wingdings");
        HFONT mono=CreateFontW(-16,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,FF_MODERN,L"Consolas");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(symbol),FALSE);check(Current(control)==symbol,"native symbol fonts retain their application handle");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(mono),FALSE);check(Current(control)==mono,"native monospaced fonts retain their application handle");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(late),FALSE);
        g_roundedSelectionHighContrast=true;NativeUiFontRefresh();check(Current(control)==late,"high contrast restores the native application font");
        g_roundedSelectionHighContrast=false;NativeUiFontRefresh();
        NativeUiFontRefresh(true);NativeUiFontRelease();
        check(Current(control)==late&&GetObjectW(Current(control),sizeof(description),&description)==sizeof(description)&&!GetPropW(control,kNativeUiFontObserved),"unload restores a live original font before deleting replacement caches");
        DestroyWindow(root);DeleteObject(original);DeleteObject(late);DeleteObject(symbol);DeleteObject(mono);
    }
    for(const auto* engine:{L"CabinetWClass",L"UnknownApplication"}){
        HWND root=CreateWindowExW(WS_EX_NOACTIVATE,engine,L"Own negative scope",WS_POPUP,-30000,-30000,100,50,nullptr,nullptr,instance,nullptr);
        HWND control=CreateWindowExW(0,L"STATIC",L"Native caption",WS_CHILD,0,0,80,20,root,nullptr,instance,nullptr);
        HFONT original=CreateFontW(-15,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(original),FALSE);
        NativeUiFontObserve(control,L"Static");check(Current(control)==original&&!GetPropW(control,kNativeUiFontObserved),"Explorer and unknown application roots remain outside installer typography observation");
        DestroyWindow(root);DeleteObject(original);
    }
    check(GetForegroundWindow()==foreground,"font regression never shows windows or takes foreground");
    return failures?1:0;
}
