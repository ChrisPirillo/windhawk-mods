// Hosts the real Windows ACL page against an in-memory, read-only descriptor.
// It never reads or writes file, account, registry or machine permissions.
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <aclui.h>
#include <sddl.h>
#include <cstdio>
#include <cstring>
#include <string>
static unsigned securityWriteRequests;
class MemorySecurity final: public ISecurityInformation {
    LONG references=1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)noexcept override{
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==IID_IUnknown || iid==IID{0x965FC360,0x16FF,0x11d0,{0x91,0xCB,0x00,0xAA,0x00,0xBB,0xB7,0x23}}){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()noexcept override{return InterlockedIncrement(&references);}
    ULONG STDMETHODCALLTYPE Release()noexcept override{ULONG count=InterlockedDecrement(&references);if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetObjectInformation(PSI_OBJECT_INFO info)noexcept override{
        *info={};info->dwFlags=SI_READONLY|SI_PAGE_TITLE;
        info->pszObjectName=const_cast<LPWSTR>(L"In-memory fixture (no real object)");
        info->pszPageTitle=const_cast<LPWSTR>(L"Security");return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSecurity(SECURITY_INFORMATION,PSECURITY_DESCRIPTOR* descriptor,BOOL)noexcept override{
        return ConvertStringSecurityDescriptorToSecurityDescriptorW(L"O:SYG:SYD:(A;;FR;;;WD)",SDDL_REVISION_1,descriptor,nullptr)?S_OK:HRESULT_FROM_WIN32(GetLastError());
    }
    HRESULT STDMETHODCALLTYPE SetSecurity(SECURITY_INFORMATION,PSECURITY_DESCRIPTOR)noexcept override{securityWriteRequests++;return E_ACCESSDENIED;}
    HRESULT STDMETHODCALLTYPE GetAccessRights(const GUID*,DWORD,PSI_ACCESS* rights,ULONG* count,ULONG* selected)noexcept override{
        static SI_ACCESS entries[]={{nullptr,FILE_ALL_ACCESS,L"Full control",SI_ACCESS_GENERAL},
            {nullptr,FILE_GENERIC_READ|FILE_GENERIC_WRITE|FILE_GENERIC_EXECUTE|DELETE,L"Modify",SI_ACCESS_GENERAL},
            {nullptr,FILE_GENERIC_READ|FILE_GENERIC_EXECUTE,L"Read & execute",SI_ACCESS_GENERAL},
            {nullptr,FILE_GENERIC_READ,L"Read",SI_ACCESS_GENERAL},
            {nullptr,FILE_GENERIC_WRITE,L"Write",SI_ACCESS_GENERAL}};
        *rights=entries;*count=ARRAYSIZE(entries);*selected=3;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE MapGeneric(const GUID*,UCHAR*,ACCESS_MASK* mask)noexcept override{
        GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};
        MapGenericMask(mask,&mapping);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetInheritTypes(PSI_INHERIT_TYPE* types,ULONG* count)noexcept override{*types=nullptr;*count=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE PropertySheetPageCallback(HWND,UINT,SI_PAGE_TYPE)noexcept override{return S_OK;}
};
static HWND sheet,checklist;
static FILE* evidence;
static bool lifecycle;
static unsigned stage,failures;
static ULONGLONG started;
static BOOL CALLBACK FindChecklist(HWND window,LPARAM){wchar_t name[80]{};GetClassNameW(window,name,80);if(!_wcsicmp(name,L"CHECKLIST_ACLUI"))checklist=window;return TRUE;}
static bool Check(bool modern){
    checklist=nullptr;EnumChildWindows(sheet,FindChecklist,0);
    bool good=checklist && IsWindowVisible(checklist);
    bool subclass=checklist && GetPropW(checklist,L"Win32UIModernizer.AclChecklistSubclass");
    RECT rect{};if(checklist)GetClientRect(checklist,&rect);
    HDC screen=checklist?GetDC(checklist):nullptr,memory=screen?CreateCompatibleDC(screen):nullptr;
    HBITMAP bitmap=memory?CreateCompatibleBitmap(screen,rect.right,rect.bottom):nullptr;
    HGDIOBJ old=bitmap?SelectObject(memory,bitmap):nullptr;
    bool rendered=bitmap && PrintWindow(checklist,memory,PW_CLIENTONLY);
    unsigned white=0,dark=0;
    if(rendered)for(int y=0;y<rect.bottom;y++)for(int x=0;x<rect.right;x++){
        COLORREF pixel=GetPixel(memory,x,y);
        if(GetRValue(pixel)>=240 && GetGValue(pixel)>=240 && GetBValue(pixel)>=240)white++;
        if(GetRValue(pixel)<=64 && GetGValue(pixel)<=64 && GetBValue(pixel)<=64)dark++;
    }
    double count=double(rect.right)*rect.bottom;
    const double whiteFraction=count?white/count:1,darkFraction=count?dark/count:0;
    good=good && rendered && !securityWriteRequests && subclass==modern && (modern?whiteFraction<0.10: whiteFraction>0.50);
    fprintf(evidence,"%s real aclui permission list stage=%u modern=%d subclass=%d rendered=%d white_fraction=%.6f dark_fraction=%.6f security_write_requests=%u\n",good?"PASS":"FAIL",stage,modern,subclass,rendered,whiteFraction,darkFraction,securityWriteRequests);fflush(evidence);
    if(old)SelectObject(memory,old);if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(screen)ReleaseDC(checklist,screen);
    return good;
}
static void CALLBACK Tick(HWND window,UINT,UINT_PTR timer,DWORD){
    if(FILE* abort=fopen("lifecycle-abort.txt","rb")){fclose(abort);failures++;KillTimer(window,timer);DestroyWindow(window);return;}
    if(GetTickCount64()-started>120000){failures++;KillTimer(window,timer);DestroyWindow(window);return;}
    if(lifecycle && stage){std::string name="lifecycle-go-"+std::to_string(stage)+".txt";FILE* signal=fopen(name.c_str(),"rb");if(!signal)return;fclose(signal);}
    if(!Check(!lifecycle || stage==0 || stage==2))failures++;
    if(lifecycle){FILE* phase=fopen("lifecycle-phase.txt","wb");if(phase){fprintf(phase,"%u",stage);fclose(phase);}if(++stage<4)return;}
    KillTimer(window,timer);DestroyWindow(window);
}
int main(int argc,char** argv){
    lifecycle=argc>1 && !strcmp(argv[1],"--lifecycle");
    evidence=fopen("modernizer-fixture-results.txt","wb");if(!evidence)return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    Sleep(1500);
    auto* security=new MemorySecurity;HPROPSHEETPAGE page=CreateSecurityPage(security);security->Release();if(!page)return 3;
    PROPSHEETHEADERW header{};header.dwSize=sizeof(header);header.dwFlags=PSH_MODELESS|PSH_NOAPPLYNOW;header.pszCaption=L"Read-only Windows ACL rendering fixture";header.nPages=1;header.phpage=&page;
    sheet=reinterpret_cast<HWND>(PropertySheetW(&header));if(!sheet || sheet==reinterpret_cast<HWND>(-1))return 3;
    ShowWindow(sheet,SW_SHOWNOACTIVATE);UpdateWindow(sheet);
    started=GetTickCount64();SetTimer(sheet,1,750,Tick);
    MSG message{};while(IsWindow(sheet) && GetMessageW(&message,nullptr,0,0)>0){if(!PropSheet_IsDialogMessage(sheet,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    fprintf(evidence,"RESULT failures=%u security_write_requests=%u\n",failures,securityWriteRequests);fclose(evidence);CoUninitialize();return failures?1:0;
}


