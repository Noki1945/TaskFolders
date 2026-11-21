#include "devices.h"
#include <filesystem>
#include <string>
namespace {
HANDLE process{};
DWORD processId{};
}
void startDevices() {
    if(FindWindowW(L"WirelessBatteryWnd",nullptr)) return;
    if(process && WaitForSingleObject(process,0)==WAIT_TIMEOUT) return;
    if(process) { CloseHandle(process); process=nullptr; }
    std::wstring path(32768,0);
    auto n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if(!n || n>=path.size()) return;
    path.resize(n);
    auto exe=std::filesystem::path(path).parent_path()/L"BatteryHub"/L"BatteryHub.exe";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION info{};
    if(CreateProcessW(exe.c_str(),nullptr,nullptr,nullptr,FALSE,0,nullptr,exe.parent_path().c_str(),&startup,&info)) {
        process=info.hProcess; processId=info.dwProcessId;
        CloseHandle(info.hThread);
    }
}
void stopDevices() {
    if(!process) return;
    HWND monitor=FindWindowW(L"WirelessBatteryWnd",nullptr);
    DWORD pid{};
    GetWindowThreadProcessId(monitor,&pid);
    if(monitor && pid==processId && WaitForSingleObject(process,0)==WAIT_TIMEOUT) PostMessageW(monitor,WM_CLOSE,0,0);
    CloseHandle(process);
}
