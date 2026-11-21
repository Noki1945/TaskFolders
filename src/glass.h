#pragma once
#include "ui.h"
bool startGlass();
void stopGlass();
bool chooseBackground(HWND owner,AppFolder& folder);
void paintGlass(HDC dc,RECT area,const AppFolder& folder,bool dark);
void setGlassShape(HWND w,int width,int height,int radius);
