#ifndef WINDOWSGDI_H
#define WINDOWSGDI_H
#include <Windows.h>
#include <gdiplus.h>
//use  cstring
#include <atlstr.h>
#include "Intercept.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Gdi32.lib")

namespace BXC_MediaLibrary {
    class WindowsGDI : public Intercept {
    public:
        explicit WindowsGDI(const char* capture, int width, int height);
        ~WindowsGDI();
    public:
        virtual bool init(int idx);
        virtual bool getFrame(uint8_t* buffer, int& size, int64_t& timestamp);//gdi->rgb
    private:
        HWINSTA mHWinSta = nullptr;
        HWND    mHWnd = nullptr;//窗口句柄
        HDC     mWndHDC = nullptr;
        ULONG_PTR mGdiplusToken;

        HDC     mCaptureHDC = nullptr;
        HBITMAP mCaptureHBitmap = nullptr;

        BITMAPINFO mBmpInfo;//位图头信息结构体
        int        mRGB_size = 0;
        PRGBTRIPLE mRGB = nullptr;

    };
}
#endif // WINDOWSGDI_H
