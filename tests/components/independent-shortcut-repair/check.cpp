#define UNICODE
#define _UNICODE
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cwchar>
namespace WinUI { static constexpr float ControlCornerRadius=4; }
static std::unordered_map<std::wstring,int> integers;
static std::unordered_map<std::wstring,std::wstring> strings;
static bool explorer=true,regedit=false,mmc=false;
static int Wh_GetIntSetting(const wchar_t* name){auto i=integers.find(name);return i==integers.end()?0:i->second;}
static const wchar_t* Wh_GetStringSetting(const wchar_t* name){return strings[name].c_str();}
namespace WindhawkUtils { struct StringSetting { const wchar_t* value;StringSetting(const wchar_t* v):value(v){} operator const wchar_t*()const{return value;} const wchar_t* get()const{return value;} }; }
static bool IsCurrentProcessExplorer(){return explorer;}
static bool IsCurrentProcessRegedit(){return regedit;}
static bool IsCurrentProcessMMC(){return mmc;}
static bool InstallerProcessMatches(const wchar_t*,const wchar_t*){return false;}
static std::atomic<int> g_animEnabled{-1};
#include "settings.inc"
#include "loader.inc"

int main(){
 unsigned failures=0;
 auto check=[&](bool pass,const char* name){printf("%s %s\n",pass?"PASS":"FAIL",name);failures+=!pass;};
 HWND foreground=GetForegroundWindow();
 for(int general:{0,1})for(int section:{0,1})for(int shortcut:{0,1}){
   integers.clear();strings.clear();
   integers[L"GeneralSection.Enabled"]=general;
   integers[L"ExplorerSection.Enabled"]=section;
   integers[L"ExplorerSection.ModernShortcutOverlay"]=shortcut;
   integers[L"ExplorerSection.ModernizeShellIcons"]=1;
   integers[L"ExplorerSection.ShellTextDirectWrite"]=1;
   integers[L"ExplorerSection.NeutralSelection"]=1;
   integers[L"GeneralSection.ModernSeparators"]=1;
   const auto* before=GetSettings();const auto old=*before;
   auto returned=LoadSettings();const auto* now=GetSettings();
   printf("matrix general=%d explorer=%d shortcut=%d\n",general,section,shortcut);
   check(now->ModernShortcutOverlay==shortcut,"shortcut value survives both category gates without enabling by default");
   check(now->ModernizeShellIcons==section&&now->ShellTextDirectWrite==section&&now->NeutralSelection==section,"other Explorer options continue to obey the Explorer category");
   check(now->ModernSeparators==general,"General options continue to obey the General category");
   check(now!=before&&before->ModernShortcutOverlay==old.ModernShortcutOverlay&&returned.ModernShortcutOverlay==old.ModernShortcutOverlay,"reload publishes a new immutable snapshot and returns the prior state");
 }
 integers.clear();strings.clear();LoadSettings();
 check(!GetSettings()->ModernShortcutOverlay,"missing original shortcut key keeps the existing off default");
 integers[L"ExplorerSection.ModernShortcutOverlay"]=1;LoadSettings();
 check(GetSettings()->ModernShortcutOverlay,"original persisted key enables independent arrows");
 integers[L"ExplorerSection.ModernShortcutOverlay"]=0;LoadSettings();
 check(!GetSettings()->ModernShortcutOverlay,"original persisted key disables arrows on the next reload");
 for(const auto* scope:{L"",L"dialogs",L"application_windows",L"unknown"}){
  strings[L"GeneralSection.OpeningAnimationScope"]=scope;LoadSettings();
  check(GetSettings()->OpeningAnimationAllApplications==!wcscmp(scope,L"application_windows"),"new opening scope is opt-in and unknown/missing values retain prior coverage");
 }
 check(GetForegroundWindow()==foreground,"settings-only check never creates windows or takes focus");
 return failures?1:0;
}
