#define UNICODE
#define _UNICODE
#include <windows.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdio>
#include <thread>

static bool denyProperty=false;
static unsigned frameCallbacks=0;
static bool destroyOnFrame=false;
static BOOL OwnSetProp(HWND window,LPCWSTR key,HANDLE value){
    if(denyProperty){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    return SetPropW(window,key,value);
}
#define SetPropW OwnSetProp
#ifdef REJECTED
#include "rejected.inc"
static void MmcStyleStripApply(HWND window,int index,LONG_PTR bit){
    LONG_PTR style=GetWindowLongPtrW(window,index);
    if(style&bit){
        SetWindowLongPtrW(window,index,style&~bit);
        SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        MmcStyleStripTrack(window,index,bit);
    }
}
static constexpr wchar_t kMmcStyleGeneration[]=L"W32M.MmcStyleGeneration";
#else
#include "ownership.inc"
#endif
#undef SetPropW

static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_NCCALCSIZE){
        ++frameCallbacks;
        if(destroyOnFrame){destroyOnFrame=false;DestroyWindow(window);return 0;}
    }
    return DefWindowProcW(window,message,wp,lp);
}
int main(){
    unsigned failures=0;
    auto check=[&](bool result,const char* label){printf("%s %s\n",result?"PASS":"FAIL",label);failures+=!result;};
    HWND foreground=GetForegroundWindow();
    HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW type{};type.hInstance=instance;type.lpfnWndProc=WindowProc;type.lpszClassName=L"OwnHiddenMmcStyleOwner";
    if(!RegisterClassW(&type))return 2;
    auto create=[&](){return CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_CLIENTEDGE,type.lpszClassName,L"Own hidden style fixture",WS_POPUP|WS_BORDER,-30000,-30000,120,80,nullptr,nullptr,instance,nullptr);};
    HWND window=create();if(!window)return 2;
    auto has=[&](int index,LONG_PTR bit){return (GetWindowLongPtrW(window,index)&bit)!=0;};
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    MmcStyleStripApply(window,GWL_EXSTYLE,WS_EX_CLIENTEDGE);
    check(!has(GWL_STYLE,WS_BORDER)&&!has(GWL_EXSTYLE,WS_EX_CLIENTEDGE),"both tracked native border bits are removed");
    check(GetPropW(window,kMmcStyleGeneration)!=nullptr,"style mutation carries a window lifetime ownership marker");
    MmcStyleStripRemoveAll();
    check(has(GWL_STYLE,WS_BORDER)&&has(GWL_EXSTYLE,WS_EX_CLIENTEDGE),"unload restores every owned style index on the same native window");
    check(!GetPropW(window,kMmcStyleGeneration)&&g_mmcStyleStripped.empty(),"unload releases ownership marker and records");

    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    RemovePropW(window,kMmcStyleGeneration);
    frameCallbacks=0;MmcStyleStripRemoveAll();
    check(!has(GWL_STYLE,WS_BORDER)&&frameCallbacks==0,"lost ownership cannot mutate or reframe a live window");
    // Explicit ownership loss models the missing marker after HWND reuse. It
    // does not assert that Windows recycled a handle during this short test.
    SetWindowLongPtrW(window,GWL_STYLE,GetWindowLongPtrW(window,GWL_STYLE)|WS_BORDER);
    denyProperty=true;frameCallbacks=0;
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    denyProperty=false;
    check(has(GWL_STYLE,WS_BORDER)&&frameCallbacks==0&&g_mmcStyleStripped.empty(),"marker allocation failure leaves native style and geometry alone");

    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    HANDLE oldGeneration=GetPropW(window,kMmcStyleGeneration);
    RemovePropW(window,kMmcStyleGeneration);
    SetWindowLongPtrW(window,GWL_STYLE,GetWindowLongPtrW(window,GWL_STYLE)|WS_BORDER);
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    HANDLE newGeneration=GetPropW(window,kMmcStyleGeneration);
    check(oldGeneration&&newGeneration&&oldGeneration!=newGeneration,"a new ownership lifetime gets a different generation");
    frameCallbacks=0;MmcStyleStripRemoveAll();
    check(has(GWL_STYLE,WS_BORDER)&&frameCallbacks==1&&!GetPropW(window,kMmcStyleGeneration),"stale generation is skipped while the current generation restores once");
    DestroyWindow(window);

    window=create();if(!window)return 2;
    frameCallbacks=0;destroyOnFrame=true;
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    check(!IsWindow(window),"native frame callback may destroy the owned control during mutation");
    MmcStyleStripRemoveAll();
    check(g_mmcStyleStripped.empty(),"destroyed control records are discarded without applying styles");
    MmcStyleStripApply(nullptr,GWL_STYLE,WS_BORDER);MmcStyleStripRemoveAll();
    check(g_mmcStyleStripped.empty(),"null window is never enrolled");
    window=create();if(!window)return 2;
#ifndef REJECTED
    std::thread worker([&]{MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);});worker.join();
    check(has(GWL_STYLE,WS_BORDER)&&g_mmcStyleStripped.empty(),"another thread cannot mutate a control through the creation-time adapter");
#endif
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    SetWindowLongPtrW(window,GWL_STYLE,GetWindowLongPtrW(window,GWL_STYLE)|WS_BORDER);
    MmcStyleStripApply(window,GWL_STYLE,WS_BORDER);
    check(g_mmcStyleStripped.size()==1,"repeated strip in one lifetime keeps only one restoration record");
#ifndef REJECTED
    MmcStyleStripBeginUnload();
    MmcStyleStripApply(window,GWL_EXSTYLE,WS_EX_CLIENTEDGE);
    check(has(GWL_EXSTYLE,WS_EX_CLIENTEDGE)&&g_mmcStyleStripped.size()==1,"before-uninit boundary rejects new creation-time mutations");
#endif
    MmcStyleStripRemoveAll();
    check(has(GWL_STYLE,WS_BORDER)&&g_mmcStyleStripped.empty(),"before-uninit still allows existing ownership to restore");
    DestroyWindow(window);
    check(GetForegroundWindow()==foreground,"all native windows stay hidden and foreground is unchanged");
    return failures?1:0;
}
