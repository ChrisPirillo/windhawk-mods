#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <cerrno>
struct Answer {bool valid=false;bool present=false;DWORD error=0;unsigned count=0;};
static Answer Inspect(DWORD target,const wchar_t* name){
    Answer answer;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,target);
    if(snapshot==INVALID_HANDLE_VALUE){answer.error=GetLastError();return answer;}
    MODULEENTRY32W entry{sizeof(entry)};
    if(!Module32FirstW(snapshot,&entry)){answer.error=GetLastError();CloseHandle(snapshot);return answer;}
    do{
        ++answer.count;
        if(!_wcsicmp(entry.szModule,name)){answer.present=true;answer.valid=true;break;}
        SetLastError(ERROR_SUCCESS);
    }while(Module32NextW(snapshot,&entry));
    if(!answer.present){answer.error=GetLastError();answer.valid=answer.count>0&&answer.error==ERROR_NO_MORE_FILES;}
    CloseHandle(snapshot);return answer;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3||!argv[1][0]||!argv[2][0])return 2;
    wchar_t* end=nullptr;errno=0;unsigned long parsed=wcstoul(argv[1],&end,10);
    if(!end||*end||errno==ERANGE||argv[1][0]==L'-')return 2;
    DWORD target=parsed?parsed:GetCurrentProcessId();
    const ULONGLONG started=GetTickCount64();Answer answer;
    do{
        answer=Inspect(target,argv[2]);
        if(answer.valid){puts(answer.present?"present":"absent");return 0;}
        // Loader startup/unload can temporarily invalidate snapshots. Never
        // convert any failed or empty enumeration into an "absent" result.
        if(answer.error!=ERROR_BAD_LENGTH&&answer.error!=ERROR_PARTIAL_COPY&&
            answer.error!=ERROR_NO_MORE_FILES)break;
        if(GetTickCount64()-started>=2000)break;
        Sleep(20);
    }while(true);
    fprintf(stderr,"Native module inspection failed: pid=%lu error=%lu count=%u elapsed=%llu ms\n",
        target,answer.error,answer.count,GetTickCount64()-started);
    return 2;
}
