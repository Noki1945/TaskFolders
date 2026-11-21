#pragma once
#include <windows.h>
#include "settings.h"
extern Folders folders;
extern std::filesystem::path config;
extern HWND bar, popup;
extern HINSTANCE instance;
void edit(HWND owner, int folder,bool devices=false);
void layout();
int runTaskbar();
RECT workArea();
void launch(HWND owner,const std::wstring& path);
HICON appIcon(const std::wstring& path);
void clearIcons();
bool supported(const std::wstring& path);