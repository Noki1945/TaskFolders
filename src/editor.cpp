#include "ui.h"
#include "devices.h"
#include "glass.h"
#include "media.h"
#include <commctrl.h>
#include "../resources/ids.h"
#include <commdlg.h>
#include <shellapi.h>
namespace {
struct Editor { Folders draft; int selected; bool devices; };
void section(HWND w,int page) {
    bool devices=page==1;
    CheckDlgButton(w,FOLDER_TAB,page==0?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(w,DEVICE_TAB,page==1?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(w,MEDIA_TAB,page==2?BST_CHECKED:BST_UNCHECKED);
    mediaControls(w,page==2);
    for(int id:{GROUP_LABEL,NAME_LABEL,APPS_LABEL,DROP_HINT,GROUPS,NAMEBOX,NEWGROUP,RENAMEGROUP,
                DROPGROUP,REMOVE_ALL,APPS,ADDAPP,DROPAPP,DENSEGRID,SHOWLABEL,BACKGROUND,BACKGROUND_FILE})
        ShowWindow(GetDlgItem(w,id),page==0?SW_SHOW:SW_HIDE);
    for(int id:{DEVICE_TREE,DEVICE_ACTION,DEVICE_STATUS,DEVICE_REFRESH})
        ShowWindow(GetDlgItem(w,id),devices?SW_SHOW:SW_HIDE);
    DragAcceptFiles(w,page==0);
    if(devices) { EnableWindow(GetDlgItem(w,DEVICE_ACTION),FALSE); openDevicePage(w); }
    else closeDevicePage();
}
void refresh(HWND w, Editor& e) {
    auto msg=[&](int id,UINT m,WPARAM a=0,LPARAM b=0){return SendDlgItemMessageW(w,id,m,a,b);};
    msg(GROUPS,LB_RESETCONTENT);
    msg(APPS,LB_RESETCONTENT);
    for(auto& f:e.draft) msg(GROUPS,LB_ADDSTRING,0,(LPARAM)f.name.c_str());
    bool selected=e.selected>=0 && e.selected<(int)e.draft.size();
    for(int id:{ADDAPP,DROPAPP,RENAMEGROUP,DROPGROUP}) EnableWindow(GetDlgItem(w,id),selected);
    SetDlgItemTextW(w,NAMEBOX,selected?e.draft[e.selected].name.c_str():L"");
    EnableWindow(GetDlgItem(w,SHOWLABEL),selected);
    CheckDlgButton(w,SHOWLABEL,selected && e.draft[e.selected].showLabel?BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(w,DENSEGRID),selected);
    CheckDlgButton(w,DENSEGRID,selected && e.draft[e.selected].pageSize==16 ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(w,BACKGROUND),selected); EnableWindow(GetDlgItem(w,BACKGROUND_FILE),selected);
    int bg=!selected || e.draft[e.selected].background.empty()?0:e.draft[e.selected].background==L"*"?1:2;
    msg(BACKGROUND,CB_SETCURSEL,bg);
    if(!selected) return;
    msg(GROUPS,LB_SETCURSEL,e.selected);
    for(auto& path:e.draft[e.selected].apps) msg(APPS,LB_ADDSTRING,0,(LPARAM)path.c_str());
}
void add(Editor& e,const std::wstring& path) {
    if(e.selected<0 || !supported(path)) return;
    auto& apps=e.draft[e.selected].apps;
    if(std::find(apps.begin(),apps.end(),path)==apps.end()) apps.push_back(path);
}
INT_PTR CALLBACK dialog(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    auto e=reinterpret_cast<Editor*>(GetWindowLongPtrW(w,DWLP_USER));
    if(m==WM_INITDIALOG) {
        e=reinterpret_cast<Editor*>(lp);
        SetWindowLongPtrW(w,DWLP_USER,lp);
        SendDlgItemMessageW(w,NAMEBOX,EM_SETLIMITTEXT,100,0);
        for(auto title:{L"Фон: стекло",L"Фон: иконка первого приложения",L"Фон: своё изображение"})
            SendDlgItemMessageW(w,BACKGROUND,CB_ADDSTRING,0,(LPARAM)title);
        CheckDlgButton(w,AUTOSTART,autoStartEnabled()?BST_CHECKED:BST_UNCHECKED);
        section(w,e->devices);
        refresh(w,*e);
        return TRUE;
    }
    if(!e) return FALSE;
    if(m==WM_HOTKEY) { mediaCommand((int)wp); return TRUE; }
    if(m==WM_DESTROY) closeDevicePage();
    if(devicePageMessage(w,m,wp,lp)) return TRUE;
    if(m==WM_DROPFILES) {
        auto drop=(HDROP)wp;
        UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);
        for(UINT i=0;i<count;i++) {
            std::wstring path(DragQueryFileW(drop,i,nullptr,0)+1,0);
            DragQueryFileW(drop,i,path.data(),static_cast<UINT>(path.size()));
            path.pop_back();
            add(*e,path);
        }
        DragFinish(drop);
        refresh(w,*e);
        return TRUE;
    }
    if(m!=WM_COMMAND) return FALSE;
    if(mediaMessage(w,wp)) return TRUE;
    switch(LOWORD(wp)) {
    case MEDIA_TAB: section(w,2); return TRUE;
    case REMOVE_ALL: e->draft.clear(); e->selected=-1; break;
    case FOLDER_TAB: case DEVICE_TAB:
        e->devices=LOWORD(wp)==DEVICE_TAB; section(w,e->devices); return TRUE;
    case BACKGROUND_FILE:
        if(e->selected>=0) chooseBackground(w,e->draft[e->selected]);
        break;
    case BACKGROUND: {
        if(HIWORD(wp)!=CBN_SELCHANGE || e->selected<0) return FALSE;
        auto& f=e->draft[e->selected];
        switch(SendDlgItemMessageW(w,BACKGROUND,CB_GETCURSEL,0,0)) {
        case 0: f.background.clear(); break;
        case 1: f.background=L"*"; break;
        case 2: chooseBackground(w,f); break;
        }
        break;
    }
    case SHOWLABEL:
        if(e->selected>=0) e->draft[e->selected].showLabel=IsDlgButtonChecked(w,SHOWLABEL)==BST_CHECKED;
        return TRUE;
    case DENSEGRID:
        if(e->selected>=0) e->draft[e->selected].pageSize=IsDlgButtonChecked(w,DENSEGRID)==BST_CHECKED?16:4;
        return TRUE;
    case GROUPS:
        if(HIWORD(wp)!=LBN_SELCHANGE) return FALSE;
        e->selected=(int)SendDlgItemMessageW(w,GROUPS,LB_GETCURSEL,0,0);
        break;
    case NEWGROUP: case RENAMEGROUP: {
        wchar_t name[101]{};
        GetDlgItemTextW(w,NAMEBOX,name,101);
        if(std::wstring(name).find_first_not_of(L" \t")==std::wstring::npos) {
            SetFocus(GetDlgItem(w,NAMEBOX)); return TRUE;
        }
        if(LOWORD(wp)==NEWGROUP) {
            e->draft.push_back({name,{}});
            e->selected=(int)e->draft.size()-1;
        } else if(e->selected>=0) e->draft[e->selected].name=name;
        break;
    }
    case DROPGROUP:
        if(e->selected>=0 && MessageBoxW(w,L"Убрать папку вместе с её списком приложений?",L"TaskFolders",MB_YESNO|MB_DEFBUTTON2)==IDYES) {
            e->draft.erase(e->draft.begin()+e->selected);
            e->selected=std::min(e->selected,(int)e->draft.size()-1);
        }
        break;
    case ADDAPP: {
        wchar_t path[32768]{};
        OPENFILENAMEW pick{sizeof pick};
        pick.hwndOwner=w;
        pick.lpstrFilter=L"Программы и ярлыки\0*.exe;*.lnk;*.url\0";
        pick.lpstrFile=path;
        pick.nMaxFile=32768;
        pick.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_NODEREFERENCELINKS;
        if(GetOpenFileNameW(&pick)) add(*e,path);
        break;
    }
    case DROPAPP: {
        int idx=(int)SendDlgItemMessageW(w,APPS,LB_GETCURSEL,0,0);
        if(e->selected>=0 && idx>=0) e->draft[e->selected].apps.erase(e->draft[e->selected].apps.begin()+idx);
        break;
    }
    case IDOK:
        if(!applyMedia(w)) return TRUE;
        if(!setAutoStart(IsDlgButtonChecked(w,AUTOSTART)==BST_CHECKED)) {
            MessageBoxW(w,L"Не удалось сохранить автозапуск.",L"TaskFolders",MB_ICONERROR);return TRUE;
        }
        try { save(config,e->draft); folders=std::move(e->draft); }
        catch(...) { MessageBoxW(w,L"Не удалось сохранить изменения. Проверьте место на диске и размер списка.",L"TaskFolders",MB_ICONERROR); return TRUE; }
        EndDialog(w,IDOK);
        return TRUE;
    case IDCANCEL: EndDialog(w,IDCANCEL); return TRUE;
    default: return FALSE;
    }
    refresh(w,*e);
    return TRUE;
}
}
void edit(HWND owner,int index,bool devices) {
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_TREEVIEW_CLASSES|ICC_HOTKEY_CLASS};
    InitCommonControlsEx(&cc);
    Editor state{folders,folders.empty()?-1:std::clamp(index,0,(int)folders.size()-1),devices};
    DialogBoxParamW(instance,MAKEINTRESOURCEW(EDITOR),owner,dialog,(LPARAM)&state);
    layout();
}