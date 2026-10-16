#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <vssym32.h>
#include <string>
#include <cstdio>
#include <vector>
static bool g_darkModeActive=true,g_excluded=false,g_mica=false;
static bool IsClassName(HWND w,LPCWSTR name){wchar_t cls[128]{};return w&&GetClassNameW(w,cls,128)&&!_wcsicmp(cls,name);}
static BOOL CALLBACK FindTree(HWND w,LPARAM out){if(IsClassName(w,L"NamespaceTreeControl"))*reinterpret_cast<bool*>(out)=true;return TRUE;}
// Shell identity adapter matches FilePickerDialogRoot's documented structural
// preconditions; no actual DirectUI code or Windhawk injection is emulated.
static HWND FilePickerRoot(HWND w){HWND root=GetAncestor(w,GA_ROOT);bool tree=false;if(!g_excluded&&IsClassName(root,L"#32770")&&FindWindowExW(root,nullptr,L"DUIViewWndClassName",nullptr))EnumChildWindows(root,FindTree,reinterpret_cast<LPARAM>(&tree));return tree?root:nullptr;}
#include "header-scope.inc"
static HWND paintWindow=nullptr;
static HWND ResolvePaintHwndFreshFirst(HDC){return paintWindow;}
static COLORREF FilePickerControlBackground(bool){return g_mica?RGB(44,44,44):RGB(43,43,43);}
static bool FilePickerMicaActive(){return g_mica;}
static HBRUSH GetCachedSolidBrush(COLORREF c){static HBRUSH a=CreateSolidBrush(RGB(43,43,43)),b=CreateSolidBrush(RGB(44,44,44));return c==RGB(43,43,43)?a:b;}
static auto FillRect_orig=FillRect;
#include "native-pixel.inc"
#include "header-paint.inc"
int main(){
 unsigned failed=0,passed=0;auto check=[&](bool value,const char* text){printf("%s %s\n",value?"PASS":"FAIL",text);value?passed++:failed++;};
 HWND foreground=GetForegroundWindow();
 INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES};InitCommonControlsEx(&ic);
 HINSTANCE instance=GetModuleHandleW(nullptr);
 auto cls=[&](LPCWSTR n){WNDCLASSW c{};c.lpfnWndProc=DefWindowProcW;c.hInstance=instance;c.lpszClassName=n;RegisterClassW(&c);};
 for(LPCWSTR n:{L"DUIViewWndClassName",L"NamespaceTreeControl",L"WorkerW",L"TravelBand",L"SearchEditBoxWrapperClass",L"SHELLDLL_DefView",L"DirectUIHWND",L"CabinetWClass"})cls(n);
 auto child=[&](HWND p,LPCWSTR n,int x,int y,int w,int h){return CreateWindowExW(0,n,L"",WS_CHILD,x,y,w,h,p,nullptr,instance,nullptr);};
 HWND root=CreateWindowExW(0,L"#32770",L"Own hidden picker chrome test",WS_POPUP,-20000,-20000,800,600,nullptr,nullptr,instance,nullptr);
 HWND worker=child(root,L"WorkerW",0,0,800,55);
 HWND rebar=child(worker,REBARCLASSNAMEW,0,0,800,55);
 HWND travel=child(rebar,L"TravelBand",0,0,100,50);
 HWND toolbar=child(travel,TOOLBARCLASSNAMEW,0,0,100,40);
 HWND search=child(rebar,L"SearchEditBoxWrapperClass",600,0,190,45);
 HWND host=child(root,L"DUIViewWndClassName",0,55,800,460);
 HWND dui=child(host,L"DirectUIHWND",0,0,800,460);
 check(!FilePickerHeaderChromeRoot(rebar),"header remains native before positive late-built picker identity");
 HWND tree=child(dui,L"NamespaceTreeControl",0,30,160,430);
 HWND defview=child(dui,L"SHELLDLL_DefView",165,30,635,430);
 HWND list=child(defview,L"DirectUIHWND",0,0,635,430);
 HWND footer=child(root,L"Edit",100,520,600,25);
 check(FilePickerHeaderChromeRoot(worker)==root,"own top WorkerW classified");
 check(FilePickerHeaderChromeRoot(rebar)==root,"own top rebar classified");
 check(FilePickerHeaderChromeRoot(toolbar)==root,"navigation toolbar classified through ancestry");
 check(FilePickerHeaderChromeRoot(search)==root,"search wrapper classified through ancestry");
 for(HWND w:{root,host,dui,tree,defview,list,footer})check(!FilePickerHeaderChromeRoot(w),"content, root, and footer never classified as header");
 HWND unusual=child(tree,L"WorkerW",0,0,100,20);check(!FilePickerHeaderChromeRoot(unusual),"nested content WorkerW cannot expand scope");
 HWND lower=child(root,L"WorkerW",0,300,100,50);check(!FilePickerHeaderChromeRoot(lower),"lower WorkerW excluded geometrically");
 MoveWindow(worker,0,0,800,100,FALSE);check(!FilePickerHeaderChromeRoot(toolbar),"overlapping in-construction header is not guessed");MoveWindow(worker,0,0,800,55,FALSE);
 g_excluded=true;check(!FilePickerHeaderChromeRoot(toolbar),"excluded process chrome remains native");g_excluded=false;
 g_darkModeActive=false;check(!FilePickerHeaderChromeRoot(toolbar),"light palette remains native");g_darkModeActive=true;
 HWND explorer=CreateWindowExW(0,L"CabinetWClass",L"Own hidden native Explorer boundary",WS_POPUP,-20000,-20000,800,600,nullptr,nullptr,instance,nullptr);
 HWND expworker=child(explorer,L"WorkerW",0,0,800,55),exprebar=child(expworker,REBARCLASSNAMEW,0,0,800,55);
 child(explorer,L"DUIViewWndClassName",0,55,800,460);child(explorer,L"NamespaceTreeControl",0,55,150,460);
 check(!FilePickerHeaderChromeRoot(exprebar),"Explorer root never enters picker chrome scope");
 HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=40;bi.bmiHeader.biHeight=-40;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* pixels=nullptr;
 HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);HGDIOBJ old=SelectObject(dc,bitmap);RECT r{0,0,40,40};
 paintWindow=toolbar;
 for(bool mica:{false,true}){g_mica=mica;
  for(const std::wstring c:{L"Rebar",L"Toolbar",L"Navigation",L"AddressBand",L"SearchBox"}){
   memset(pixels,0xE0,40*40*4);check(PaintFilePickerHeaderBackground(dc,c,0,0,&r),"known common background drawn independently of Explorer category");
   GdiFlush();check(GetPixel(dc,20,20)==FilePickerControlBackground(true),"background pixel matches footer palette");
   if(mica)check(static_cast<DWORD*>(pixels)[20*40+20]==0xFF2C2C2Cu,"Mica header pixels are opaque instead of backdrop addition");
  }
  check(PaintFilePickerHeaderBackground(dc,L"Rebar",RP_BAND,0,&r),"rebar band common background accepted");
  check(PaintFilePickerHeaderBackground(dc,L"Rebar",RP_BACKGROUND,0,&r),"rebar background accepted");
  check(!PaintFilePickerHeaderBackground(dc,L"Toolbar",TP_BUTTON,TS_HOT,&r),"hover visuals stay native");
  check(!PaintFilePickerHeaderBackground(dc,L"Navigation",1,2,&r),"pressed/navigation glyph parts stay native");
  check(!PaintFilePickerHeaderBackground(dc,L"AddressBand",1,1,&r),"address control field treatment stays native");
  check(!PaintFilePickerHeaderBackground(dc,L"ItemsView",0,0,&r),"item theme never handled");
  paintWindow=list;check(!PaintFilePickerHeaderBackground(dc,L"Toolbar",0,0,&r),"shell list DC never handled");paintWindow=toolbar;
 }
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);DestroyWindow(root);DestroyWindow(explorer);
 check(GetForegroundWindow()==foreground,"foreground unchanged; no window shown or activated");
 printf("RESULT passed=%u failed=%u\n",passed,failed);return failed?1:0;
}
