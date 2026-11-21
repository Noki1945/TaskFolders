#include <windows.h>
#include <shellapi.h>
#include "ui.h"
#include "devices.h"
#include "glass.h"
#include <shlobj.h>
Folders folders;
std::filesystem::path config;
HINSTANCE instance;
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int) {
    instance=h;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    auto mutex=CreateMutexW(nullptr,FALSE,L"Local\\TaskFolders.Overlay.v1");
    if(!mutex || GetLastError()==ERROR_ALREADY_EXISTS) {
        if(mutex) CloseHandle(mutex);
        return 0;
    }
    auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=1;
    try {
        PWSTR local=nullptr;
        if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))) throw std::runtime_error("LocalAppData unavailable");
        config=std::filesystem::path(local)/L"TaskFolders"/L"folders.dat";
        CoTaskMemFree(local);
        int argc=0;
        auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        bool portable=argv && argc==2 && std::wstring_view(argv[1])==L"--portable";
        LocalFree(argv);
        if(portable) {
            std::wstring exe(32768,0);
            auto count=GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()));
            if(!count || count==exe.size()) throw std::runtime_error("Executable path unavailable");
            exe.resize(count);
            config=std::filesystem::path(exe).parent_path()/L"folders.dat";
        }
        folders=load(config);
        if(!startGlass()) throw std::runtime_error("Graphics unavailable");
        startDevices();
        result=runTaskbar();
        stopDevices();
        stopGlass();
    } catch(...) {
        MessageBoxW(nullptr,L"Не удалось открыть TaskFolders. Проверьте файл %LOCALAPPDATA%\\TaskFolders\\folders.dat.",L"TaskFolders",MB_ICONERROR);
    }
    clearIcons();
    if(SUCCEEDED(com)) CoUninitialize();
    CloseHandle(mutex);
    return result;
}