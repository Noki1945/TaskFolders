#include "ui.h"
#include "glass.h"
#include "media.h"
#include "../resources/ids.h"
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <algorithm>
HWND bar{},popup{};
namespace {
constexpr UINT openTimer=1,closeTimer=2;
constexpr int tile=92, row=80, pad=4, header=34,footer=32;
int strip=44;
int first{},visible{},active=-1,hover=-1,page{},cols=3,rows=3;
bool pinned{},editing{},dark{};
UINT shellRestart;
HWND controller{},taskbar{},tray{},tasks{},battery{};
HWINEVENTHOOK shellEvents{};
constexpr UINT refreshTimer=3;
constexpr UINT refreshMessage=WM_APP+1;
bool quitting{},noticeVisible{};
constexpr UINT noticeMessage=WM_APP+2;
void notice(bool needed) {
    if(needed==noticeVisible) return;
    NOTIFYICONDATAW data{sizeof(data)};
    data.hWnd=controller;
    data.uID=1;
    data.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;
    data.uCallbackMessage=noticeMessage;
    data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(data.szTip,folders.empty()?L"TaskFolders · создать папку":L"TaskFolders · недостаточно места на панели задач");
    if(!needed) { Shell_NotifyIconW(NIM_DELETE,&data); noticeVisible=false; }
    else noticeVisible=Shell_NotifyIconW(NIM_ADD,&data)!=FALSE;
}
LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
void attach();
void CALLBACK shellEvent(HWINEVENTHOOK,DWORD,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(object!=OBJID_WINDOW || child!=CHILDID_SELF || !w) return;
    if(w!=taskbar && w!=tray && w!=tasks && w!=battery) {
        if(GetParent(w)!=taskbar) return;
        wchar_t cls[64]{};
        GetClassNameW(w,cls,64);
        if(wcscmp(cls,L"WirelessBatteryWidget")) return;
    }
    PostMessageW(controller,refreshMessage,0,0);
}
HFONT font{};
COLORREF surface,ink,accent,muted;
int px(int v) { return MulDiv(v,bar ? GetDpiForWindow(bar):96,96); }
void theme() {
    DWORD value=1,size=sizeof value;
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size);
    dark=value==0;
    surface=dark?RGB(35,38,44):RGB(246,248,251);
    ink=dark?RGB(244,245,247):RGB(30,37,49);
    muted=dark?RGB(57,62,72):RGB(229,234,242);
    accent=dark?RGB(134,181,255):RGB(48,103,183);
    if(font) DeleteObject(font);
    font=CreateFontW(-px(12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
}
void fill(HDC dc,RECT r,COLORREF color) {
    SetDCBrushColor(dc,color);
    FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));
}
void label(HDC dc,std::wstring_view value,RECT r,UINT align=DT_CENTER) {
    DrawTextW(dc,value.data(),(int)value.size(),&r,align|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
}
void hide() {
    if(bar) { KillTimer(bar,openTimer); KillTimer(bar,closeTimer); }
    active=-1; hover=-1; pinned=false;
    ShowWindow(popup,SW_HIDE);
    if(bar) InvalidateRect(bar,nullptr,FALSE);
}
int hitBar(int x) {
    if(x<px(pad)) return -1;
    int slot=(x-px(pad))/px(strip);
    return slot<visible?first+slot:-1;
}
void paint(HWND w) {
    PAINTSTRUCT ps{};
    HDC target=BeginPaint(w,&ps);
    RECT r{}; GetClientRect(w,&r);
    HDC dc=CreateCompatibleDC(target);
    auto bitmap=CreateCompatibleBitmap(target,r.right,r.bottom);
    auto old=SelectObject(dc,bitmap);
    auto oldFont=SelectObject(dc,font);
    fill(dc,r,w==bar?RGB(1,2,3):surface);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,ink);
    if(w==bar) {
        for(int i=0;i<visible;i++) {
            int x=px(pad)+i*px(strip);
            RECT cell{x,px(3),x+px(strip)-1,r.bottom-px(3)};
            if(first+i==active || first+i==hover) fill(dc,cell,muted);
            int mid=x+px(strip)/2;
            const auto& folder=folders[first+i];
            int top=folder.showLabel ? px(2):(r.bottom-px(23))/2;
            POINT outline[]={{mid-px(15),top},{mid-px(5),top},{mid-px(2),top+px(4)},
                {mid+px(15),top+px(4)},{mid+px(15),top+px(22)},{mid-px(15),top+px(22)}};
            auto pen=SelectObject(dc,GetStockObject(DC_PEN));
            auto brush=SelectObject(dc,GetStockObject(DC_BRUSH));
            SetDCPenColor(dc,dark?RGB(132,138,148):RGB(30,30,32));
            SetDCBrushColor(dc,dark?RGB(65,69,78):RGB(160,168,180));
            Polygon(dc,outline,6);
            SelectObject(dc,brush); SelectObject(dc,pen);
            int count=std::min(3,(int)folder.apps.size());
            int step=px(8),size=px(12),left=mid-(size+(count-1)*step)/2;
            for(int j=0;j<count;++j) {
                HICON icon=appIcon(folder.apps[j]);
                DrawIconEx(dc,left+j*step,top+px(7),icon?icon:LoadIconW(nullptr,IDI_APPLICATION),size,size,0,nullptr,DI_NORMAL);
            }
            if(folder.showLabel) label(dc,folder.name,{x,top+px(22),x+px(strip),r.bottom});
        }
    } else if(active>=0) {
        auto& f=folders[active];
        paintGlass(dc,r,f,dark);
        int cap=cols*rows, pages=std::max(1,((int)f.apps.size()+cap-1)/cap);
        auto title=f.name==L"Приложения"?std::wstring{}:f.name;
        if(pages>1) {
            RECT prev{r.right-px(83),r.bottom-px(footer),r.right-px(46),r.bottom};
            RECT next{prev.right,r.bottom-px(footer),r.right-px(6),r.bottom};
            SetTextColor(dc,page>0?ink:muted); label(dc,L"❮",prev);
            SetTextColor(dc,page+1<pages?ink:muted); label(dc,L"❯",next);
            SetTextColor(dc,ink);
            label(dc,std::to_wstring(page+1)+L" / "+std::to_wstring(pages),
                {px(12),prev.top,prev.left,r.bottom},DT_LEFT);
        }
        label(dc,title,{px(12),0,r.right-px(8),px(header)},DT_LEFT);
        if(f.apps.empty()) label(dc,L"Добавить приложения…",{0,px(header),r.right,r.bottom});
        for(int slot=0;slot<cap && page*cap+slot<(int)f.apps.size();slot++) {
            auto& path=f.apps[page*cap+slot];
            int x=px(pad)+(slot%cols)*px(tile), y=px(header)+(slot/cols)*px(row);
            HICON icon=appIcon(path);
            if(!icon) icon=LoadIconW(nullptr,IDI_APPLICATION);
            DrawIconEx(dc,x+(px(tile)-px(40))/2,y+px(4),icon,px(40),px(40),0,nullptr,DI_NORMAL);
            label(dc,std::filesystem::path(path).stem().wstring(),{x+2,y+px(46),x+px(tile)-2,y+px(row)-2});
        }
    }
    BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,oldFont); SelectObject(dc,old);
    DeleteObject(bitmap); DeleteDC(dc);
    EndPaint(w,&ps);
}
void showFolder(int idx,bool pin) {
    if(idx<0 || idx>=(int)folders.size()) return;
    if(active!=idx) page=0;
    active=idx; pinned=pin;
    KillTimer(bar,closeTimer);
    RECT b{}; GetWindowRect(bar,&b);
    auto area=workArea();
    int side=folders[idx].pageSize==16?4:2;
    cols=std::clamp((int)(area.right-area.left-px(2*pad))/px(tile),1,side);
    rows=std::clamp((int)(area.bottom-area.top-px(header+footer+pad))/px(row),1,side);
    int width=cols*px(tile)+px(2*pad);
    int height=rows*px(row)+px(header+pad);
    if((int)folders[idx].apps.size()>cols*rows) height+=px(footer);
    setGlassShape(popup,width,height,px(28));
    int x=b.left+px(pad)+(idx-first)*px(strip)+(px(strip)-width)/2;
    x=std::clamp(x,(int)area.left,std::max((int)area.left,(int)area.right-width));
    bool wasVisible=IsWindowVisible(popup)!=FALSE;
    int y=b.top-height-px(2);
    if(y<area.top) y=b.bottom+px(2);
    SetWindowPos(popup,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);
    BOOL animate=TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);
    if(!wasVisible && animate) AnimateWindow(popup,90,AW_BLEND);
    SetLayeredWindowAttributes(popup,0,228,LWA_ALPHA);
    ShowWindow(popup,SW_SHOWNOACTIVATE);
    if(pin) { SetForegroundWindow(popup); SetFocus(popup); }
    InvalidateRect(popup,nullptr,FALSE);
    InvalidateRect(bar,nullptr,FALSE);
}
void editor(int idx,bool devices=false) {
    editing=true; hide(); edit(controller,idx,devices); editing=false;
}
void menu(int target=-1) {
    editing=true; int idx=target>=0?target:(active<0?first:active); hide();
    HMENU m=CreatePopupMenu();
    AppendMenuW(m,MF_STRING,1,L"Настройки…");
    if(idx>=0 && idx<(int)folders.size()) {
        auto& f=folders[idx];
        AppendMenuW(m,MF_STRING,3,L"Фон папки: выбрать картинку…");
        AppendMenuW(m,MF_STRING|(f.apps.empty()?MF_GRAYED:0)|(f.background==L"*"?MF_CHECKED:0),4,L"Фон из иконки первого приложения");
        AppendMenuW(m,MF_STRING|(f.background.empty()?MF_CHECKED:0),5,L"Стекло без картинки");
    }
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    AppendMenuW(m,MF_STRING,2,L"Выход");
    POINT p{}; GetCursorPos(&p); SetForegroundWindow(controller);
    int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,controller,nullptr);
    DestroyMenu(m); editing=false;
    if(cmd>=3 && cmd<=5 && idx>=0 && idx<(int)folders.size()) {
        auto copy=folders;
        if(cmd==3 && !chooseBackground(controller,copy[idx])) return;
        if(cmd==4) copy[idx].background=L"*";
        if(cmd==5) copy[idx].background.clear();
        try { save(config,copy); folders=std::move(copy); }
        catch(...) { MessageBoxW(controller,L"Не удалось сохранить фон.",L"TaskFolders",MB_ICONERROR); }
    }
    if(cmd==1) editor(idx);
    if(cmd==2) PostMessageW(controller,WM_CLOSE,0,0);
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    if(w==controller) {
        if(m==WM_HOTKEY) { mediaCommand((int)wp); return 0; }
        static const UINT openDevices=RegisterWindowMessageW(L"TaskFolders.OpenDevices.1");
        if(m==openDevices) {
            HWND dialog=GetLastActivePopup(controller);
            if(editing && dialog!=controller) {
                SetForegroundWindow(dialog); PostMessageW(dialog,WM_COMMAND,DEVICE_TAB,0);
            } else if(!editing) editor(first,true);
            return 0;
        }
        if(shellRestart && m==shellRestart) noticeVisible=false;
        if(m==WM_SETTINGCHANGE) theme();
        if(m==noticeMessage) {
            if(lp==WM_RBUTTONUP) menu();
            if(lp==WM_LBUTTONUP) editor(first);
            return 0;
        }
        if((shellRestart && m==shellRestart) || m==WM_DISPLAYCHANGE || m==WM_SETTINGCHANGE || m==refreshMessage) {
            if(!quitting) SetTimer(w,refreshTimer,80,nullptr);
            return 0;
        }
        if(m==WM_TIMER && wp==refreshTimer) {
            KillTimer(w,refreshTimer); attach(); return 0;
        }
        if(m==WM_CLOSE) {
            quitting=true;
            notice(false);
            if(shellEvents) UnhookWinEvent(shellEvents);
            if(bar) DestroyWindow(bar);
            DestroyWindow(w);
            return 0;
        }
        if(m==WM_DESTROY) PostQuitMessage(0);
        return DefWindowProcW(w,m,wp,lp);
    }
    switch(m) {
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED_AFTERPARENT: case WM_DPICHANGED:
        if(w==bar && popup) { theme(); layout(); }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paint(w); return 0;
    case WM_MOUSEACTIVATE:
        if(w==bar) return MA_NOACTIVATE;
        break;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,w,0}; TrackMouseEvent(&track);
        if(editing) return 0;
        KillTimer(bar,closeTimer);
        if(w==bar && !pinned) {
            int idx=hitBar(GET_X_LPARAM(lp));
            if(idx!=hover) {
                hover=idx; KillTimer(bar,openTimer);
                if(idx>=0 && idx!=active) SetTimer(bar,openTimer,200,nullptr);
                if(idx<0 && active>=0) SetTimer(bar,closeTimer,300,nullptr);
                InvalidateRect(bar,nullptr,FALSE);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if(w==bar) { KillTimer(bar,openTimer); hover=-1; InvalidateRect(bar,nullptr,FALSE); }
        if(!pinned && active>=0) SetTimer(bar,closeTimer,300,nullptr);
        return 0;
    case WM_TIMER: {
        KillTimer(bar,wp);
        POINT p{}; GetCursorPos(&p);
        HWND under=WindowFromPoint(p);
        if(wp==openTimer && under==bar && !editing) showFolder(hover,false);
        if(wp==closeTimer && !pinned && under!=popup) {
            ScreenToClient(bar,&p);
            if(under!=bar || hitBar(p.x)<0) hide();
        }
        return 0;
    }
    case WM_ACTIVATE:
        if(w==popup && LOWORD(wp)==WA_INACTIVE && pinned && (HWND)lp!=bar) hide();
        break;
    case WM_KEYDOWN: if(wp==VK_ESCAPE) hide(); return 0;
    case WM_RBUTTONUP:
        if(w==bar) { menu(hitBar(GET_X_LPARAM(lp))); return 0; }
        break;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        ScreenToClient(w,&point);
        menu(w==bar?hitBar(point.x):active);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int step=GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1;
        if(w==bar) { hide(); first=std::clamp(first+step,0,(int)folders.size()-visible); layout(); }
        else if(active>=0) {
            page=std::clamp(page+step,0,std::max(0,((int)folders[active].apps.size()-1)/(cols*rows)));
            InvalidateRect(w,nullptr,FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        if(w==bar) {
            int idx=hitBar(x);
            if(idx<0) return 0;
            else if(active==idx && pinned) hide();
            else showFolder(idx,true);
        } else if(active>=0 && y>=px(header)) {
            RECT bounds{}; GetClientRect(w,&bounds);
            int capacity=cols*rows;
            int pages=((int)folders[active].apps.size()+capacity-1)/capacity;
            if(pages>1 && y>=bounds.bottom-px(footer)) {
                if(x>=bounds.right-px(83) && x<bounds.right-px(6)) {
                    page=std::clamp(page+(x<bounds.right-px(46)?-1:1),0,pages-1);
                    InvalidateRect(w,nullptr,FALSE);
                }
                return 0;
            }
            if(folders[active].apps.empty()) { editor(active); return 0; }
            int col=(x-px(pad))/px(tile), line=(y-px(header))/px(row);
            int i=(page*rows+line)*cols+col;
            if(x>=px(pad) && col<cols && line<rows && i<(int)folders[active].apps.size()) {
                auto path=folders[active].apps[i]; hide(); launch(controller,path);
            }
        }
        return 0;
    }
    case WM_CLOSE: PostMessageW(controller,WM_CLOSE,0,0); return 0;
    case WM_DESTROY:
        if(w==bar) {
            hide(); bar=nullptr;
            if(!quitting) PostMessageW(controller,refreshMessage,0,0);
        }
        return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
namespace {
void attach() {
    HWND host=FindWindowW(L"Shell_TrayWnd",nullptr);
    if(host!=taskbar || !IsWindow(bar)) {
        if(bar && IsWindow(bar)) DestroyWindow(bar);
        if(shellEvents) { UnhookWinEvent(shellEvents); shellEvents=nullptr; }
        taskbar=host;
        if(host) {
            shellEvents=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,
                nullptr,shellEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
            bar=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_NOPARENTNOTIFY,L"TaskFolders.Surface",
                L"TaskFolders · Папки панели задач",WS_CHILD|WS_CLIPSIBLINGS,0,0,1,1,host,nullptr,instance,nullptr);
            if(bar) SetLayeredWindowAttributes(bar,RGB(1,2,3),215,LWA_COLORKEY|LWA_ALPHA);
        }
    }
    if(!bar) { if(host) notice(true); SetTimer(controller,refreshTimer,1500,nullptr); return; }
    tray=FindWindowExW(host,nullptr,L"TrayNotifyWnd",nullptr);
    HWND rebar=FindWindowExW(host,nullptr,L"ReBarWindow32",nullptr);
    tasks=rebar ? FindWindowExW(rebar,nullptr,L"MSTaskSwWClass",nullptr) : nullptr;
    layout();
}
}
void layout() {
    if(!bar || !popup) return;
    RECT client{}, notification{}, buttons{};
    GetClientRect(taskbar,&client);
    bool measured=tray && GetWindowRect(tray,&notification) && tasks && GetWindowRect(tasks,&buttons);
    if(measured) {
        MapWindowPoints(nullptr,taskbar,reinterpret_cast<POINT*>(&notification),2);
        MapWindowPoints(nullptr,taskbar,reinterpret_cast<POINT*>(&buttons),2);
    }
    int right=std::min(client.right,notification.left)-px(4);
    battery=FindWindowExW(taskbar,nullptr,L"WirelessBatteryWidget",nullptr);
    RECT device{};
    if(battery && IsWindowVisible(battery) && GetWindowRect(battery,&device)) {
        MapWindowPoints(nullptr,taskbar,reinterpret_cast<POINT*>(&device),2);
        if(device.right>device.left && device.bottom>0 && device.top<client.bottom)
            right=std::min(right,(int)device.left-px(4));
    }
    int available=right-std::max(0L,buttons.right);
    strip=std::any_of(folders.begin(),folders.end(),[](const AppFolder& f){return f.showLabel;})?68:44;
    int chrome=2*px(pad);
    int count=std::min((int)folders.size(),std::max(0,(available-chrome)/px(strip)));
    if(!measured || available<chrome || client.bottom>=client.right || count==0) {
        notice(true);
        if(IsWindowVisible(bar)) { hide(); ShowWindow(bar,SW_HIDE); }
        return;
    }
    notice(false);
    visible=count;
    first=std::clamp(first,0,(int)folders.size()-visible);
    int width=chrome+visible*px(strip);
    int height=std::min((int)client.bottom,px(41));
    RECT desired{right-width,(client.bottom-height)/2,right,(client.bottom-height)/2+height};
    RECT actual{};
    GetWindowRect(bar,&actual);
    MapWindowPoints(nullptr,taskbar,reinterpret_cast<POINT*>(&actual),2);
    if(!EqualRect(&actual,&desired) || !IsWindowVisible(bar) || GetWindow(bar,GW_HWNDPREV)) {
        bool moved=!EqualRect(&actual,&desired);
        POINT cursor{}; GetCursorPos(&cursor);
        bool crossing=WindowFromPoint(cursor)==popup;
        SetWindowPos(bar,HWND_TOP,desired.left,desired.top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        if(active>=0 && moved) {
            if(active<first || active>=first+visible) hide();
            else if(!crossing) showFolder(active,pinned);
        }
    }
    InvalidateRect(bar,nullptr,FALSE);
}
int runTaskbar() {
    WNDCLASSW wc{};
    wc.hInstance=instance; wc.lpfnWndProc=proc; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.lpszClassName=L"TaskFolders.Surface";
    if(!RegisterClassW(&wc)) return 1;
    controller=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"TaskFolders.Controller",
        WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
    if(!controller) return 1;
    popup=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,wc.lpszClassName,L"TaskFolders · Приложения",
        WS_POPUP,0,0,1,1,controller,nullptr,instance,nullptr);
    if(!popup) return 1;
    SetLayeredWindowAttributes(popup,0,228,LWA_ALPHA);
    DWORD round=2;
    DwmSetWindowAttribute(popup,33,&round,sizeof round);
    shellRestart=RegisterWindowMessageW(L"TaskbarCreated");
    initMedia(controller);
    attach(); theme();
    MSG msg{}; int status;
    while((status=GetMessageW(&msg,nullptr,0,0))>0) { if(msg.message==WM_HOTKEY) mediaCommand((int)msg.wParam); else { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    stopMedia();
    DeleteObject(font);
    return status<0?1:0;
}