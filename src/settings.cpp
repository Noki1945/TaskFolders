#include "settings.h"
#include <windows.h>
#include <fstream>
#include <stdexcept>
#include <cstdint>
namespace {
constexpr uint32_t signature = 0x31444654;
void number(std::ostream& out, uint32_t n) { out.write(reinterpret_cast<const char*>(&n), sizeof n); }
uint32_t read(std::istream& in, uint32_t limit) {
    uint32_t n{};
    in.read(reinterpret_cast<char*>(&n), sizeof n);
    if (!in || n > limit) throw std::runtime_error("Invalid configuration");
    return n;
}
void string(std::ostream& out, const std::wstring& s) {
    if (s.empty() || s.size()>32767) throw std::runtime_error("Invalid text length");
    number(out, static_cast<uint32_t>(s.size()));
    out.write(reinterpret_cast<const char*>(s.data()), s.size()*sizeof(wchar_t));
}
std::wstring string(std::istream& in) {
    std::wstring s(read(in,32767), L'\0');
    in.read(reinterpret_cast<char*>(s.data()), s.size()*sizeof(wchar_t));
    if (!in || s.empty() || s.find(L'\0') != s.npos) throw std::runtime_error("Invalid text");
    return s;
}
}
Folders load(const std::filesystem::path& file) {
    if (!std::filesystem::exists(file)) return {{L"Приложения",{}}};
    if (std::filesystem::file_size(file)>16*1024*1024) throw std::runtime_error("Configuration too large");
    std::ifstream in(file,std::ios::binary);
    auto version=read(in,UINT32_MAX);
    if (version!=signature && version!=0x32444654 && version!=0x33444654) throw std::runtime_error("Unknown format");
    Folders result;
    for (auto n=read(in,256); n; --n) {
        AppFolder folder{string(in),{}};
        for (auto k=read(in,4096); k; --k) folder.apps.push_back(string(in));
        if(version!=signature) {
            folder.pageSize=read(in,16);
            if(folder.pageSize!=4 && folder.pageSize!=16) throw std::runtime_error("Invalid page size");
            if(read(in,1)) folder.background=string(in);
        }
        if(version==0x33444654) folder.showLabel=read(in,1)!=0;
        result.push_back(std::move(folder));
    }
    if (in.peek()!=EOF) throw std::runtime_error("Unexpected trailing data");
    return result;
}
void save(const std::filesystem::path& file,const Folders& folders) {
    std::filesystem::create_directories(file.parent_path());
    auto temp=file;
    temp+=L".tmp";
    try {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        out.exceptions(std::ios::failbit|std::ios::badbit);
        number(out,0x33444654);
        if(folders.size()>256) throw std::runtime_error("Too many folders");
        number(out,static_cast<uint32_t>(folders.size()));
        for(const auto& folder:folders) {
            string(out,folder.name);
            if(folder.apps.size()>4096) throw std::runtime_error("Too many apps");
            number(out,static_cast<uint32_t>(folder.apps.size()));
            for(const auto& path:folder.apps) string(out,path);
            if(folder.pageSize!=4 && folder.pageSize!=16) throw std::runtime_error("Invalid page size");
            number(out,folder.pageSize);
            number(out,!folder.background.empty());
            if(!folder.background.empty()) string(out,folder.background);
            number(out,folder.showLabel);
        }
        if(out.tellp()>16*1024*1024) throw std::runtime_error("Configuration too large");
        out.close();
        if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace configuration");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp,ignored);
        throw;
    }
}