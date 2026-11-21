#pragma once
#include <windows.h>
void initMedia(HWND owner);
void stopMedia();
void mediaCommand(int id);
void mediaControls(HWND w,bool show);
bool mediaMessage(HWND w,WPARAM wp);
bool applyMedia(HWND w);
bool autoStartEnabled();
bool setAutoStart(bool enabled);
