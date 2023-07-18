#ifndef FFMPEGRECORDER_H
#define FFMPEGRECORDER_H
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include "Intercept.h"
namespace BXC_MediaLibrary {
    class FFmpegRecorder : public Intercept {
    public:
        FFmpegRecorder(const char* capture, int width, int height);
        ~FFmpegRecorder();
    public:
        virtual bool init(int idx);
        virtual bool getFrame(uint8_t* buffer, int& size, int64_t& timestamp);
    private:
        void release();
    private:
        AVFormatContext* mFmtCtx = nullptr;
        AVCodecContext* mCodecCtx = nullptr;
        AVStream* mStream = nullptr;
        int       mIndex = -1;
        AVPacket* mPkt = nullptr;
        AVFrame*  srcFrame = nullptr;

        AVFrame* dstFrame = nullptr;
        uint8_t* dstFrame_buff = nullptr;

        SwsContext* mVideoSwsCtx = nullptr;
        int dstDataSize = 0;


    };
};

#endif // FFMPEGRECORDER_H