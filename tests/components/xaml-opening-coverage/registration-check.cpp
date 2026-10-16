#define UNICODE
#include <windows.h>
#include <cstdio>
using NtUserCreateWindowEx_t=HWND(WINAPI*)(DWORD,VOID*,LPCWSTR,VOID*,DWORD,LONG,LONG,LONG,LONG,HWND,HMENU,HINSTANCE,LPVOID,DWORD,DWORD,DWORD,VOID*);
static NtUserCreateWindowEx_t NtUserCreateWindowEx_orig=nullptr;
static HWND WINAPI OpeningNtUserCreateWindowEx_hook(DWORD,VOID*,LPCWSTR,VOID*,DWORD,LONG,LONG,LONG,LONG,HWND,HMENU,HINSTANCE,LPVOID,DWORD,DWORD,DWORD,VOID*){return nullptr;}
struct{bool ModernContextMenus=false,DisableWindowOpeningAnimations=false;}g_settings;
static bool available=true,registerSuccess=true;static unsigned attempts=0,logs=0,enumerations=0;
static HMODULE ModelModule(LPCWSTR){return available?(HMODULE)1:nullptr;}
static FARPROC ModelProc(HMODULE,LPCSTR){return available?(FARPROC)0x1234:nullptr;}
namespace WindhawkUtils{static bool SetFunctionHook(NtUserCreateWindowEx_t target,decltype(&OpeningNtUserCreateWindowEx_hook) hook,NtUserCreateWindowEx_t* orig){++attempts;if(target!=(NtUserCreateWindowEx_t)0x1234||hook!=OpeningNtUserCreateWindowEx_hook||orig!=&NtUserCreateWindowEx_orig)return false;return registerSuccess;}}
static BOOL CALLBACK MenuInitEnumProc(HWND,LPARAM){return TRUE;}
static BOOL ModelEnum(WNDENUMPROC p,LPARAM l){if(p==MenuInitEnumProc&&l==0)++enumerations;return TRUE;}
static void Wh_Log(LPCWSTR){++logs;}
#define GetModuleHandleW ModelModule
#define GetProcAddress ModelProc
#define EnumWindows ModelEnum
static BOOL TerminalXamlInit(){
#include "registration.inc"
}
int main(){unsigned checks=0,errors=0;auto check=[&](bool v,const char*n){++checks;errors+=!v;printf("%s %s\n",v?"PASS":"FAIL",n);};check(TerminalXamlInit()&&attempts==1&&!logs&&!enumerations,"inactive fastpath still queues minimal creation hook for live enable");g_settings.DisableWindowOpeningAnimations=true;check(TerminalXamlInit()&&attempts==2&&enumerations==1,"menus-off opening-on enumerates existing UI threads");g_settings.DisableWindowOpeningAnimations=false;g_settings.ModernContextMenus=true;check(TerminalXamlInit()&&attempts==3&&enumerations==2,"existing menu enumeration retained");registerSuccess=false;check(TerminalXamlInit()&&attempts==4&&logs==1&&enumerations==3,"registration failure logs limitation and retains menu-only successful return");available=false;check(TerminalXamlInit()&&attempts==4&&logs==2&&enumerations==4,"missing export does not queue invalid hook or enter normal initialization");printf("checks=%u failures=%u; exact XAML terminal initializer with modeled registrar and enumerator\n",checks,errors);return errors?1:0;}
