#include "Intercept.h"
namespace BXC_MediaLibrary {
	Intercept::Intercept(const char* capture,int width, int height):
		mCapture(capture),mWidth(width), mHeight(height)
	{
	}
	Intercept::~Intercept() {
	}
	int Intercept::getWidth() {
		return mWidth;
	}
	int Intercept::getHeight() {
		return mHeight;
	}

	int Intercept::getFactWidth() {
		return mFactWidth;
	}
	int Intercept::getFactHeight() {
		return mFactHeight;
	}
	PixelFormat Intercept::getPixelFormat() {
		return mPixelFormat;
	}
	const char* Intercept::getCapture() {
		return mCapture;
	}
}