#include "settings.h"
#include <windows.h>
#include <fstream>
int main() {
    auto dir=std::filesystem::temp_directory_path()/(L"TaskFolders-test-"+std::to_wstring(GetCurrentProcessId()));
    auto path=dir/L"folders.dat";
    if(load(path).size()!=1) return 1;
    Folders expected={{L"Учёба 🎓",{L"C:\\Меню Пуск\\Word.lnk"}},{L"Games",{}}};
    expected[0].background=L"C:\\Фото\\Фон.png";
    expected[0].pageSize=16;
    expected[0].showLabel=true;
    save(path,expected);
    if(load(path)!=expected) return 2;
    try { save(path,{{L"",{}}}); return 3; } catch (...) {}
    if(load(path)!=expected) return 4;
    save(path,{});
    if(!load(path).empty()) return 5;
    std::ofstream(path,std::ios::binary)<<"bad";
    try { load(path); return 6; } catch (...) {}
    std::filesystem::remove_all(dir);
}