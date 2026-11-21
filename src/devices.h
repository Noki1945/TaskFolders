#pragma once
#include <windows.h>
void startDevices();
bool devicePageMessage(HWND dialog,UINT message,WPARAM wp,LPARAM lp);
void openDevicePage(HWND dialog);
void closeDevicePage();

void stopDevices();
