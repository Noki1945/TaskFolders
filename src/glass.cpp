#include "glass.h"
#include <objidl.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <commdlg.h>
#include <memory>
#include <algorithm>
using namespace Gdiplus;
namespace {
ULONG_PTR token{};
std::wstring cachedPath;
std::unique_ptr<Bitmap> cachedImage;
std::unique_ptr<Bitmap> thumbnail(const wchar_t* path) {
    Bitmap source(path);
    UINT w=source.GetWidth(),h=source.GetHeight();
    if(source.GetLastStatus()!=Ok || !w || !h) return {};
    double scale=std::min(1.0,768.0/std::max(w,h));
    int sw=std::max(1,int(w*scale)),sh=std::max(1,int(h*scale));
    auto bmp=std::make_unique<Bitmap>(sw,sh,PixelFormat32bppPARGB);
    Graphics canvas(bmp.get());
    canvas.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    if(canvas.DrawImage(&source,0,0,sw,sh)!=Ok) return {};
    return bmp;
}
}
bool startGlass() {
    GdiplusStartupInput input;
    return GdiplusStartup(&token,&input,nullptr)==Ok;
}
void stopGlass() {
    cachedImage.reset();
    if(token) GdiplusShutdown(token);
}
bool chooseBackground(HWND owner,AppFolder& folder) {
    wchar_t path[32768]{};
    OPENFILENAMEW file{sizeof(file)};
    file.hwndOwner=owner;
    file.lpstrFilter=L"Изображения\0*.png;*.jpg;*.jpeg;*.bmp\0";
    file.lpstrFile=path; file.nMaxFile=32768;
    file.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&file)) return false;
    auto image=thumbnail(path);
    if(!image) {
        MessageBoxW(owner,L"Не удалось прочитать изображение.",L"Фон папки",MB_ICONWARNING);
        return false;
    }
    cachedPath=path; cachedImage=std::move(image); folder.background=path;
    return true;
}
void setGlassShape(HWND w,int width,int height,int radius) {
    HRGN shape=CreateRoundRectRgn(0,0,width+1,height+1,radius,radius);
    if(shape && !SetWindowRgn(w,shape,TRUE)) DeleteObject(shape);
}
void paintGlass(HDC dc,RECT area,const AppFolder& folder,bool dark) {
    int w=area.right,h=area.bottom;
    if(w<2 || h<2) return;
    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Rect frame(0,0,w,h);
    LinearGradientBrush base(frame,dark?Color(52,64,86):Color(223,235,248),
        dark?Color(27,31,44):Color(244,235,246),LinearGradientModeForwardDiagonal);
    g.FillRectangle(&base,frame);
    if(!folder.background.empty() && folder.background!=L"*") {
        if(cachedPath!=folder.background) {
            cachedImage=thumbnail(folder.background.c_str());
            cachedPath=folder.background;
        }
        if(cachedImage) {
            float iw=float(cachedImage->GetWidth()),ih=float(cachedImage->GetHeight());
            float scale=std::max(w/iw,h/ih);
            g.DrawImage(cachedImage.get(),RectF((w-iw*scale)/2,(h-ih*scale)/2,iw*scale,ih*scale));
        }
    } else if(folder.background==L"*" && !folder.apps.empty()) {
        if(HICON icon=appIcon(folder.apps.front())) {
            Bitmap image(icon);
            int side=std::min(w,h)*4/5;
            g.DrawImage(&image,(w-side)/2,(h-side)/2,side,side);
        }
    }
    LinearGradientBrush veil(frame,dark?Color(140,31,40,56):Color(144,245,249,255),
        dark?Color(195,17,21,31):Color(196,235,241,250),LinearGradientModeVertical);
    g.FillRectangle(&veil,frame);
    Pen shine(dark?Color(124,190,212,238):Color(224,255,255,255),2.f);
    g.DrawLine(&shine,14,1,w-14,1);
}
