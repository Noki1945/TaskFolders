#pragma once
#include <filesystem>
#include <string>
#include <vector>
struct AppFolder {
    std::wstring name;
    std::vector<std::wstring> apps;
    std::wstring background; // Empty uses glass; "*" uses the first application icon.
    unsigned pageSize=4;
    bool showLabel=false;
    bool operator==(const AppFolder&) const = default;
};
using Folders = std::vector<AppFolder>;
Folders load(const std::filesystem::path& file);
void save(const std::filesystem::path& file, const Folders& folders);