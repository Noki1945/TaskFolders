#include "devices.h"
#include "../resources/ids.h"
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <set>
namespace {
const UINT query=RegisterWindowMessageW(L"TaskFolders.BatterySettings.1");
const UINT action=RegisterWindowMessageW(L"TaskFolders.BatteryAction.1");
constexpr UINT retry=96;
HWND client{},monitor{};
int attempts{};
std::wstring previous;
UINT itemId(HWND tree,HTREEITEM item) {
    TVITEMW info{TVIF_PARAM}; info.hItem=item;
    return item && TreeView_GetItem(tree,&info)?static_cast<UINT>(info.lParam):0;
}
void apply(HWND w) {
    HWND tree=GetDlgItem(w,DEVICE_TREE);
    UINT id=itemId(tree,TreeView_GetSelection(tree));
    if(id && IsWindow(monitor)) PostMessageW(monitor,action,(WPARAM)w,id);
}
void request(HWND w) {
    monitor=FindWindowW(L"WirelessBatteryWnd",nullptr);
    if(monitor) PostMessageW(monitor,query,(WPARAM)w,0);
}
void populate(HWND w,const std::wstring& rows) {
    if(previous==rows) return;
    previous=rows;
    HWND tree=GetDlgItem(w,DEVICE_TREE);
    UINT selected=itemId(tree,TreeView_GetSelection(tree));
    std::set<int> expanded;
    int position=0;
    for(auto h=TreeView_GetRoot(tree); h; h=TreeView_GetNextSibling(tree,h),++position)
        if(TreeView_GetItemState(tree,h,TVIS_EXPANDED)) expanded.insert(position);
    SendMessageW(tree,WM_SETREDRAW,FALSE,0);
    TreeView_DeleteAllItems(tree);
    std::wistringstream stream(rows);
    std::wstring line;
    std::vector<HTREEITEM> parents;
    std::vector<HTREEITEM> reopen;
    HTREEITEM restore{};
    TVINSERTSTRUCTW group{}; group.hParent=TVI_ROOT; group.hInsertAfter=TVI_LAST;
    group.item.mask=TVIF_TEXT; group.item.pszText=const_cast<wchar_t*>(L"Дополнительные настройки");
    HTREEITEM advanced{};
    position=0;
    while(std::getline(stream,line)) {
        size_t split=line.find(L'\t'), end=line.find(L'\t',split+1);
        if(split==line.npos || end==line.npos) continue;
        unsigned depth=wcstoul(line.c_str(),nullptr,10),id=wcstoul(line.c_str()+split+1,nullptr,10);
        if(depth>parents.size() || depth>8) continue;
        TVINSERTSTRUCTW entry{};
        if(!depth && id!=100 && id!=0 && !advanced) advanced=TreeView_InsertItem(tree,&group);
        entry.hParent=depth?parents[depth-1]:(id!=100 && id!=0?advanced:TVI_ROOT); entry.hInsertAfter=TVI_LAST;
        entry.item.mask=TVIF_TEXT|TVIF_PARAM; entry.item.lParam=id;
        entry.item.pszText=line.data()+end+1;
        auto h=TreeView_InsertItem(tree,&entry);
        parents.resize(depth+1); parents[depth]=h;
        if(!depth && expanded.count(position++)) reopen.push_back(h);
        if(id && id==selected) restore=h;
    }
    for(auto item:reopen) TreeView_Expand(tree,item,TVE_EXPAND);
    if(restore) TreeView_SelectItem(tree,restore);
    SendMessageW(tree,WM_SETREDRAW,TRUE,0); InvalidateRect(tree,nullptr,TRUE);
}
}
void openDevicePage(HWND w) {
    client=w; attempts=0; previous.clear();
    startDevices();
    SetDlgItemTextW(w,DEVICE_STATUS,L"Подключение к монитору устройств…");
    request(w); SetTimer(w,retry,350,nullptr);
}
void closeDevicePage() {
    if(client) KillTimer(client,retry);
    client=nullptr; previous.clear();
}
bool devicePageMessage(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(client!=w) return false;
    if(msg==WM_COPYDATA) {
        auto data=reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if((HWND)wp!=monitor || !data || data->dwData!=0x54464231 || !data->lpData || data->cbData<2 ||
           data->cbData>1024*1024 || data->cbData%sizeof(wchar_t)) return false;
        auto text=static_cast<const wchar_t*>(data->lpData);
        size_t count=data->cbData/sizeof(wchar_t);
        if(text[count-1]) return false;
        KillTimer(w,retry);
        populate(w,std::wstring(text,count-1));
        SetDlgItemTextW(w,DEVICE_STATUS,L"Раскройте устройство или раздел. Двойной щелчок по параметру применяет его сразу. Галочкой отмечено текущее значение.");
        return true;
    }
    if(msg==WM_TIMER && wp==retry) {
        if(++attempts<12) request(w);
        else {
            KillTimer(w,retry);
            SetDlgItemTextW(w,DEVICE_STATUS,L"Нет ответа от монитора. Проверьте, что запущен BatteryHub из комплекта TaskFolders. Нажмите «Устройства и заряд», чтобы повторить.");
        }
        return true;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==DEVICE_REFRESH) {
        if(IsWindow(monitor)) PostMessageW(monitor,action,(WPARAM)w,100); // BatteryHub 1.1.0 ID_REFRESH
        else openDevicePage(w);
        return true;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==DEVICE_ACTION){apply(w); return true;}
    if(msg==WM_NOTIFY) {
        auto hdr=reinterpret_cast<NMHDR*>(lp);
        if(hdr->idFrom!=DEVICE_TREE) return false;
        if(hdr->code==TVN_SELCHANGEDW) {
            auto n=reinterpret_cast<NMTREEVIEWW*>(lp);
            EnableWindow(GetDlgItem(w,DEVICE_ACTION),n->itemNew.lParam!=0);
        } else if(hdr->code==NM_DBLCLK) apply(w);
        else if(hdr->code==TVN_KEYDOWN && reinterpret_cast<NMTVKEYDOWN*>(lp)->wVKey==VK_RETURN) apply(w);
    }
    return false;
}
