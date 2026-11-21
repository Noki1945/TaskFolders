#include "media.h"
#include <commctrl.h>
#include <array>
#include <string>
namespace {
constexpr wchar_t key[]=L"Software\\TaskFolders";
constexpr wchar_t run[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr int base=700;
const wchar_t* names[]={L"Пауза / продолжить",L"Предыдущий трек",L"Следующий трек",L"Громче",L"Тише"};
constexpr WORD commands[]={VK_MEDIA_PLAY_PAUSE,VK_MEDIA_PREV_TRACK,VK_MEDIA_NEXT_TRACK,VK_VOLUME_UP,VK_VOLUME_DOWN};
std::array<WORD,5> bindings{};
bool enabled{};
HWND recipient{};
UINT modifiers(WORD binding) {
    BYTE flags=HIBYTE(binding);
    return MOD_NOREPEAT|((flags&HOTKEYF_CONTROL)?MOD_CONTROL:0)|
        ((flags&HOTKEYF_ALT)?MOD_ALT:0)|((flags&HOTKEYF_SHIFT)?MOD_SHIFT:0);
}
bool registerBindings(const std::array<WORD,5>& values,bool on) {
    if(!on) return true;
    for(int i=0;i<5;++i) if(LOBYTE(values[i])) {
        if(LOBYTE(values[i])==VK_F12 || !RegisterHotKey(recipient,base+i,modifiers(values[i]),LOBYTE(values[i]))) {
            for(int j=0;j<i;++j) UnregisterHotKey(recipient,base+j);
            return false;
        }
    }
    return true;
}
HWND control(HWND w,const wchar_t* cls,const wchar_t* text,DWORD style,int id,RECT r) {
    MapDialogRect(w,&r);
    HWND h=CreateWindowExW(0,cls,text,WS_CHILD|style,r.left,r.top,r.right-r.left,r.bottom-r.top,w,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(h,WM_SETFONT,SendMessageW(w,WM_GETFONT,0,0),TRUE);
    return h;
}
}
void stopMedia(){for(int i=0;i<5;++i) UnregisterHotKey(recipient,base+i);}
void initMedia(HWND owner) {
    recipient=owner;
    DWORD size=sizeof(bindings),on=0,onSize=sizeof(on);
    RegGetValueW(HKEY_CURRENT_USER,key,L"MediaKeys",RRF_RT_REG_BINARY,nullptr,bindings.data(),&size);
    RegGetValueW(HKEY_CURRENT_USER,key,L"MediaEnabled",RRF_RT_REG_DWORD,nullptr,&on,&onSize);
    enabled=on!=0;
    if(!registerBindings(bindings,enabled)) {
        enabled=false;
        MessageBoxW(nullptr,L"Некоторые музыкальные сочетания заняты. Откройте настройки клавиш и назначьте свободные.",L"TaskFolders",MB_ICONINFORMATION);
    }
}
void mediaCommand(int id) {
    if(id<base || id>=base+5) return;
    INPUT input[2]{};
    for(auto& event:input){event.type=INPUT_KEYBOARD;event.ki.wVk=commands[id-base];}
    input[1].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(2,input,sizeof(INPUT));
}
void mediaControls(HWND w,bool show) {
    if(!GetDlgItem(w,750)) {
        control(w,L"BUTTON",L"Включить музыкальные сочетания",BS_AUTOCHECKBOX|WS_TABSTOP,750,{10,34,280,52});
        CheckDlgButton(w,750,enabled?BST_CHECKED:BST_UNCHECKED);
        for(int i=0;i<5;++i) {
            int y=65+i*31;
            control(w,L"BUTTON",names[i],WS_TABSTOP,760+i,{10,y,145,y+22});
            HWND hotkey=control(w,HOTKEY_CLASSW,L"",WS_TABSTOP|WS_BORDER,770+i,{154,y,310,y+22});
            SendMessageW(hotkey,HKM_SETHOTKEY,bindings[i],0);
            control(w,L"BUTTON",L"Сброс",WS_TABSTOP,780+i,{320,y,378,y+22});
        }
        control(w,L"STATIC",L"Нажмите поле и задайте клавишу с Ctrl, Alt или Shift.\n«Готово» сохраняет назначения. F12 зарезервирована Windows.",0,790,{10,224,385,254});
    }
    for(int id=750;id<=790;++id) if(HWND child=GetDlgItem(w,id)) ShowWindow(child,show?SW_SHOW:SW_HIDE);
}
bool mediaMessage(HWND w,WPARAM wp) {
    int id=LOWORD(wp);
    if(id>=760 && id<765){mediaCommand(base+id-760);return true;}
    if(id>=780 && id<785){SendDlgItemMessageW(w,770+id-780,HKM_SETHOTKEY,0,0);return true;}
    return false;
}
bool applyMedia(HWND w) {
    if(!GetDlgItem(w,750)) return true;
    std::array<WORD,5> values{};
    for(int i=0;i<5;++i) values[i]=(WORD)SendDlgItemMessageW(w,770+i,HKM_GETHOTKEY,0,0);
    bool on=IsDlgButtonChecked(w,750)==BST_CHECKED;
    stopMedia();
    if(!registerBindings(values,on)) {
        registerBindings(bindings,enabled);
        MessageBoxW(w,L"Сочетание занято, повторяется или использует F12. Выберите другое.",L"Музыкальные клавиши",MB_ICONWARNING);
        return false;
    }
    DWORD flag=on;
    auto err=RegSetKeyValueW(HKEY_CURRENT_USER,key,L"MediaKeys",REG_BINARY,values.data(),sizeof(values));
    if(err==ERROR_SUCCESS) err=RegSetKeyValueW(HKEY_CURRENT_USER,key,L"MediaEnabled",REG_DWORD,&flag,sizeof(flag));
    if(err!=ERROR_SUCCESS) {
        stopMedia(); registerBindings(bindings,enabled);
        MessageBoxW(w,L"Не удалось сохранить сочетания.",L"TaskFolders",MB_ICONERROR);return false;
    }
    bindings=values;enabled=on;return true;
}
bool autoStartEnabled() {
    DWORD size=0;
    return RegGetValueW(HKEY_CURRENT_USER,run,L"TaskFolders",RRF_RT_REG_SZ,nullptr,nullptr,&size)==ERROR_SUCCESS;
}
bool setAutoStart(bool enabled) {
    LSTATUS result;
    if(enabled) {
        std::wstring command=GetCommandLineW();
        result=RegSetKeyValueW(HKEY_CURRENT_USER,run,L"TaskFolders",REG_SZ,command.c_str(),(DWORD)((command.size()+1)*sizeof(wchar_t)));
    } else {
        HKEY h{};result=RegOpenKeyExW(HKEY_CURRENT_USER,run,0,KEY_SET_VALUE,&h);
        if(result==ERROR_SUCCESS){result=RegDeleteValueW(h,L"TaskFolders");RegCloseKey(h);}
        if(result==ERROR_FILE_NOT_FOUND) return true;
    }
    return result==ERROR_SUCCESS;
}
