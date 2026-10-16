#define UNICODE
#define _UNICODE
#include <windows.h>
#include <cstdio>
static struct {bool FlatLegacyBorders=true;}g_settings;
static bool mmc=true,g_darkModeActive=true,customDark=true,highContrast=false;
static bool IsCurrentProcessMMC(){return mmc;}
static bool IsCustomDarkModeAllowed(){return customDark;}
static bool SysHighContrastEnabled(){return highContrast;}
#ifdef ORIGINAL
#include "rebar-original.inc"
#else
#include "rebar-candidate.inc"
#endif
int main(){
    unsigned failures=0;
    auto check=[&](bool result,const char* label){printf("%s %s\n",result?"PASS":"FAIL",label);failures+=!result;};
    HWND foreground=GetForegroundWindow();
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return 2;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=16;info.bmiHeader.biHeight=-8;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap)return 2;
    HGDIOBJ oldBitmap=SelectObject(dc,bitmap),oldPen=GetCurrentObject(dc,OBJ_PEN);RECT rect{0,0,16,8};
    auto paint=[&](COLORREF background){HBRUSH brush=CreateSolidBrush(background);FillRect(dc,&rect,brush);DeleteObject(brush);DrawRebarBandLine(dc,&rect);};
    paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(31,34,37),"MMC flat-border path leaves the exact prefilled background on bottom row");
    check(GetPixel(dc,6,6)==RGB(31,34,37),"client interior pixels stay unchanged");
    paint(RGB(17,19,23));
    check(GetPixel(dc,6,7)==RGB(17,19,23),"flat path does not hardcode a replacement background shade");
    g_settings.FlatLegacyBorders=false;paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(48,48,48),"disabled setting preserves original explicit separator");
    g_settings.FlatLegacyBorders=true;mmc=false;paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(48,48,48),"non-MMC policy unchanged");
    mmc=true;g_darkModeActive=false;paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(48,48,48),"inactive dark mode policy unchanged");
    g_darkModeActive=true;customDark=false;paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(48,48,48),"custom-dark exclusion policy unchanged");
    customDark=true;highContrast=true;paint(RGB(31,34,37));
    check(GetPixel(dc,6,7)==RGB(48,48,48),"high-contrast path not changed by new flat policy");
    check(GetCurrentObject(dc,OBJ_PEN)==oldPen,"GDI pen ownership preserved for every policy path");
    SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);
    check(GetForegroundWindow()==foreground,"DIB-only check never opens or activates a window");
    return failures?1:0;
}
