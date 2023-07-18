#ifndef WINDOWSDXGI_H
#define WINDOWSDXGI_H
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <sal.h>
#include <new>
#include <DirectXMath.h>
#include <memory>
#include <algorithm>
#include "Intercept.h"

#pragma comment(lib,"d3d11.lib")
namespace BXC_MediaLibrary {
    class WindowsDXGI : public Intercept {

    public:
        WindowsDXGI(const char* capture, int width, int height);
        ~WindowsDXGI();
    public:
        virtual bool init(int idx);
        virtual bool getFrame(uint8_t* buffer, int& size, int64_t& timestamp);//dxgi->bgra
    private:
        bool initDevice();
        bool initDuplication(int idx);
        bool getFrame(int timeout = 100); //millisecond
        bool copyFrameData(uint8_t* buffer, int& size);
        bool doneWithFrame();
    private:
        ID3D11Device* m_Device;
        IDXGIOutputDuplication* m_DeskDupl;
        ID3D11Texture2D* m_AcquiredDesktopImage;
        UINT m_OutputIdx;

    };
};
#endif // WINDOWSDXGI_H
