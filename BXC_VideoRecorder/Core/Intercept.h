#ifndef INTERCEPT_H
#define INTERCEPT_H
#include <string>
namespace BXC_MediaLibrary {

    enum PixelFormat {
        PIXEL_FMT_UNKNOWN = 20,
        PIXEL_FMT_BGRA,  //BGRA
        PIXEL_FMT_RGB,   //RGB
    };

    class Intercept
    {
    public:
        explicit Intercept(const char* capture,int width, int height);
        virtual ~Intercept();
    public:
        virtual bool init(int idx) = 0;
        virtual bool getFrame(uint8_t* buffer, int& size, int64_t& timestamp) = 0;
        
    public:
        int getWidth();
        int getHeight();
        int getFactWidth();
        int getFactHeight();

        PixelFormat getPixelFormat();
        const char* getCapture();
    protected:
        int mWidth;//参数传递过来的宽度
        int mHeight;//参数传递过来的高度
        
        int mFactWidth;//事实宽度
        int mFactHeight;//事实宽度

        PixelFormat mPixelFormat = PIXEL_FMT_UNKNOWN;
        const char* mCapture = "unknown";
    };
}
#endif // INTERCEPT_H
