#include "WindowsGDI.h"
#include "Common.h"
namespace BXC_MediaLibrary {
	WindowsGDI::WindowsGDI(const char* capture, int width, int height) : Intercept(capture, width, height)
	{
		mPixelFormat = PixelFormat::PIXEL_FMT_RGB;
	}
	WindowsGDI::~WindowsGDI() {
		if (mRGB) {
			free(mRGB);
			mRGB = nullptr;
		}
		if (mCaptureHBitmap) {
			DeleteObject(mCaptureHBitmap);
		}
		if (mCaptureHDC) {
			DeleteDC(mCaptureHDC);
		}

		//卸载GDI
		Gdiplus::GdiplusShutdown(mGdiplusToken);

		if (mHWnd) {
			ReleaseDC(mHWnd, mWndHDC);
		}

	}
	bool WindowsGDI::init(int idx) {

		mHWinSta = GetProcessWindowStation();
		if (!mHWinSta) {
			LOGE("%s GetProcessWindowStation error", getCapture());
			return false;
		}

		mHWnd = GetDesktopWindow();//GetActiveWindow();
		mWndHDC = GetDC(mHWnd);

		//初始化GDI
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		if (Gdiplus::GdiplusStartup(&mGdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
		{
			LOGE("%s Gdiplus::GdiplusStartup error", getCapture());
			return false;
		}

		mCaptureHDC = CreateCompatibleDC(mWndHDC);
		mCaptureHBitmap = CreateCompatibleBitmap(mWndHDC, mWidth, mHeight);
		SelectObject(mCaptureHDC, mCaptureHBitmap);//将位图选入创建好的兼容句柄

		
		mBmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		mBmpInfo.bmiHeader.biWidth = mWidth;
		mBmpInfo.bmiHeader.biHeight = -mHeight;
		mBmpInfo.bmiHeader.biPlanes = 1;
		mBmpInfo.bmiHeader.biBitCount = 24;
		mBmpInfo.bmiHeader.biCompression = BI_RGB;
		mBmpInfo.bmiHeader.biSizeImage = 0;
		mBmpInfo.bmiHeader.biXPelsPerMeter = 0;
		mBmpInfo.bmiHeader.biYPelsPerMeter = 0;
		mBmpInfo.bmiHeader.biClrUsed = 0;
		mBmpInfo.bmiHeader.biClrImportant = 0;

		mRGB_size = mWidth * mHeight * 3;//24位图像大小
		mRGB = (PRGBTRIPLE)malloc(mRGB_size);

		//通过接口获取屏幕宽高，不准确
		//mFactWidth = GetSystemMetrics(SM_CXSCREEN);
		//mFactHeight = GetSystemMetrics(SM_CYSCREEN);

		mFactWidth = mWidth;
		mFactHeight = mHeight;
		return true;
	}

	bool WindowsGDI::getFrame(uint8_t* buffer, int& size, int64_t& timestamp) {
		timestamp = getCurTimestamp();
		BitBlt(mCaptureHDC, 0, 0, mWidth, mHeight, mWndHDC, 0, 0, SRCCOPY);

		GetDIBits(mCaptureHDC,mCaptureHBitmap,
			0, mHeight, mRGB, (LPBITMAPINFO)&mBmpInfo, DIB_RGB_COLORS);

		memcpy(buffer, mRGB, mRGB_size);
		size = mRGB_size;

		return true;
	}
}