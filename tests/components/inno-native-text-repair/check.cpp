#define UNICODE
#define _UNICODE
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
enum class InstallerEngine{None,Inno};
static bool enabled=true,contrast=false,inno=true;
static bool IsCustomDarkModeAllowed(){return enabled;}
static bool SysHighContrastEnabled(){return contrast;}
static InstallerEngine InstallerWindowEngine(HWND){return inno?InstallerEngine::Inno:InstallerEngine::None;}
static bool InstallerNeutralSurface(COLORREF c){int low=std::min({GetRValue(c),GetGValue(c),GetBValue(c)}),high=std::max({GetRValue(c),GetGValue(c),GetBValue(c)});return low>=160&&high-low<=12;}
static HWND t_installerPaintWindow;
static bool g_glowEntry;
static constexpr COLORREF Win32MainDark=RGB(32,32,32);
namespace WinUI{namespace Dark{static constexpr COLORREF TextFillColorPrimary=RGB(255,255,255);}}
static decltype(&SetBkColor) SetBkColor_orig=SetBkColor;
#include "scope.inc"
int main(){
 unsigned errors=0;auto check=[&](bool b,const char*label){printf("%s %s\n",b?"PASS":"FAIL",label);errors+=!b;};
 HWND foreground=GetForegroundWindow();HINSTANCE instance=GetModuleHandleW(nullptr);
 WNDCLASSW cls{};GetClassInfoW(nullptr,L"STATIC",&cls);cls.hInstance=instance;cls.lpszClassName=L"TNewStaticText";RegisterClassW(&cls);
 GetClassInfoW(nullptr,L"BUTTON",&cls);cls.hInstance=instance;cls.lpszClassName=L"TNewRadioButton";RegisterClassW(&cls);
 HWND root=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"Own native caption check",WS_POPUP,-30000,-30000,200,80,nullptr,nullptr,instance,nullptr);
 HWND label=CreateWindowExW(0,L"TNewStaticText",L"caption g j p",WS_CHILD,0,0,180,25,root,nullptr,instance,nullptr);
 HWND radio=CreateWindowExW(0,L"TNewRadioButton",L"Radio caption",WS_CHILD|BS_RADIOBUTTON,0,30,180,25,root,nullptr,instance,nullptr);
 HWND unknown=CreateWindowExW(0,L"EDIT",L"document-like custom route",WS_CHILD,0,0,180,25,root,nullptr,instance,nullptr);
 HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=200;info.bmiHeader.biHeight=-80;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto previous=SelectObject(dc,bitmap);
 auto font=CreateFontW(-16,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(dc,font);
 t_installerPaintWindow=label;SetBkColor(dc,RGB(240,240,240));SetTextColor(dc,RGB(0,0,0));SetBkMode(dc,OPAQUE);
 RECT text{0,0,190,30};RECT measured=text;int nativeHeight=DrawTextW(dc,L"caption g j p",-1,&measured,DT_CALCRECT|DT_NOPREFIX);
 {
  InstallerNativeCaptionScope scope(dc);
  check(scope.Active()&&g_glowEntry,"known neutral Inno caption enters native rendering scope");
  check(GetBkColor(dc)==Win32MainDark&&GetTextColor(dc)==RGB(255,255,255),"native caption has coherent dark opaque background and readable foreground");
  RECT changed=text;int height=DrawTextW(dc,L"caption g j p",-1,&changed,DT_CALCRECT|DT_NOPREFIX);
  check(height==nativeHeight&&memcmp(&changed,&measured,sizeof(RECT))==0,"native text measurement and caller rectangle contract preserved");
  RECT fill{0,0,200,80};ExtTextOutW(dc,0,0,ETO_OPAQUE,&fill,nullptr,0,nullptr);
  check(GetPixel(dc,190,70)==Win32MainDark,"opaque-only native fill cannot restore a pale caption cell");
  POINT before{},after{};MoveToEx(dc,4,40,nullptr);GetCurrentPositionEx(dc,&before);SetTextAlign(dc,TA_UPDATECP);ExtTextOutW(dc,0,0,0,nullptr,L"abc",3,nullptr);GetCurrentPositionEx(dc,&after);SetTextAlign(dc,TA_LEFT);
  check(after.x>before.x,"native current-position advance remains observable");
  DTTOPTS authored{sizeof(authored)};authored.dwFlags=DTT_TEXTCOLOR|DTT_GLOWSIZE;authored.crText=RGB(190,40,20);authored.iGlowSize=3;auto original=authored;auto value=scope.ThemeOptions(&authored);
  check(value.crText==authored.crText&&value.iGlowSize==3&&memcmp(&authored,&original,sizeof(authored))==0,"authored theme colors and source options remain intact");
  authored.crText=RGB(0,0,0);value=scope.ThemeOptions(&authored);check(value.crText==RGB(255,255,255),"neutral theme-black caption receives the selected readable color");
  authored.crText=RGB(157,157,157);value=scope.ThemeOptions(&authored);check(value.crText==authored.crText,"explicit disabled or secondary gray theme caption remains gray");
 }
 check(!g_glowEntry&&GetBkColor(dc)==RGB(240,240,240)&&GetTextColor(dc)==RGB(0,0,0)&&GetBkMode(dc)==OPAQUE,"scope restores caller colors, mode and reentry state");
 check(GetCurrentObject(dc,OBJ_FONT)==font,"scope preserves selected font");
 t_installerPaintWindow=radio;EnableWindow(radio,FALSE);
 {InstallerNativeCaptionScope scope(dc);check(scope.Active()&&GetTextColor(dc)==RGB(157,157,157),"disabled radio caption uses a single neutral disabled foreground");}
 EnableWindow(radio,TRUE);t_installerPaintWindow=unknown;{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"edits, RichEdit and unknown VCL painters do not acquire the caption policy");}
 t_installerPaintWindow=label;SetBkColor(dc,RGB(110,40,150));{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"authored colored caption surfaces retain their original pipeline");}SetBkColor(dc,RGB(240,240,240));
 enabled=false;{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"option off leaves native application colors unchanged");}enabled=true;
 contrast=true;{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"high contrast does not acquire the dark caption policy");}contrast=false;
 inno=false;{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"other applications do not acquire the Inno caption policy");}inno=true;
 t_installerPaintWindow=nullptr;{InstallerNativeCaptionScope scope(dc);check(!scope.Active(),"unowned temporary DC never guesses an installer caption");}
 SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);DestroyWindow(root);
 check(GetForegroundWindow()==foreground,"fixture never activates or controls external windows");return errors?1:0;
}
