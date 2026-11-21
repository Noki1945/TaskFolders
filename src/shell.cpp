#include "ui.h"
#include <shellapi.h>
#include <unordered_map>
namespace { std::unordered_map<std::wstring,HICON> cache; }
bool supported(const std::wstring& path) {
    auto ext=std::filesystem::path(path).extension().wstring();
    return !_wcsicmp(ext.c_str(),L".lnk") || !_wcsicmp(ext.c_str(),L".exe") || !_wcsicmp(ext.c_str(),L".url");
}
void launch(HWND owner,const std::wstring& path) {
    SHELLEXECUTEINFOW action{sizeof(action)};
    action.hwnd=owner;
    action.fMask=SEE_MASK_FLAG_NO_UI;
    action.lpFile=path.c_str();
    auto dir=std::filesystem::path(path).parent_path();
    action.lpDirectory=dir.c_str();
    action.nShow=SW_SHOWNORMAL;
    if(!supported(path) || GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES || !ShellExecuteExW(&action))
        MessageBoxW(owner,(L"Не удалось открыть приложение. Проверьте ярлык или добавьте его заново.\n\n"+path).c_str(),L"TaskFolders",MB_ICONWARNING);
}
HICON appIcon(const std::wstring& path) {
    if(auto it=cache.find(path); it!=cache.end()) return it->second;
    SHFILEINFOW info{};
    SHGetFileInfoW(path.c_str(),0,&info,sizeof info,SHGFI_ICON|SHGFI_LARGEICON);
    if(cache.size()>=256) clearIcons();
    cache[path]=info.hIcon;
    return info.hIcon;
}
void clearIcons() {
    for(auto& item:cache) if(item.second) DestroyIcon(item.second);
    cache.clear();
}
RECT workArea() {
    APPBARDATA data{sizeof(data)};
    bool known=SHAppBarMessage(ABM_GETTASKBARPOS,&data)!=0;
    POINT origin{};
    auto monitor=known ? MonitorFromRect(&data.rc,MONITOR_DEFAULTTOPRIMARY) : MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(monitor,&mi);
    RECT r=mi.rcWork;
    if(known) {
        if(data.uEdge==ABE_BOTTOM) r.bottom=std::min(r.bottom,data.rc.top);
        if(data.uEdge==ABE_TOP) r.top=std::max(r.top,data.rc.bottom);
        if(data.uEdge==ABE_LEFT) r.left=std::max(r.left,data.rc.right);
        if(data.uEdge==ABE_RIGHT) r.right=std::min(r.right,data.rc.left);
    }
    return r;
}