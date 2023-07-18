#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H
extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace BXC_MediaLibrary {
    struct BXC_AvEncoder;
    struct BXC_AvFrame;
    struct BXC_AvPacket;

    enum PixelFormat {
        PIXEL_FMT_UNKNOWN = 20,
        PIXEL_FMT_BGRA,  //BGRA
        PIXEL_FMT_RGB,   //RGB
    };

    class FFmpegEncoder
    {
    public:
        FFmpegEncoder();
        ~FFmpegEncoder();
    public:
        bool start(BXC_AvEncoder* encoder, const char* url);
        bool stop();
        static int64_t gettime();

        void sendFrame(BXC_AvFrame* frame);//添加待编码帧
        BXC_AvFrame*  getVideoFrame(); //获取待编码视频帧
        BXC_AvFrame*  getAudioFrame(); //获取待编码音频帧

        void sendPacket(BXC_AvPacket* packet);
        BXC_AvPacket* getVideoPacket();//获取编码后的视频帧
        BXC_AvPacket* getAudioPacket();//获取编码后的音频帧

        bool getHasVideo();
        bool getHasAudio();
    private:
        static void startThread(void* arg);
        bool handle();

        bool encodeVideoFrame(BXC_AvFrame* videoFrame);
        bool encodeAudioFrame(BXC_AvFrame* audioFrame);

        void writePkt(AVPacket* pkt);
    
        void clearVideoFrame();
        void clearAudioFrame();
        void clearVideoPacket();
        void clearAudioPacket();
    private:
        bool mContainWrite = false;//是否包含写入本地文件
        bool mHasVideo = false;
        bool mHasAudio = false;

        bool mIsStop = true;
        std::thread* mThread = nullptr;//编码线程

        AVFormatContext* mFmtCtx = nullptr;

        //视频处理start
        int      mVideoSrcWidth = 0;
        int      mVideoSrcHeight = 0;
        int      mVideoSrcFps = 0;
        int      mVideoSrcChannel = 0;
        int      mVideoSrcStride[1] = {0};
        
        AVCodecContext*  mVideoCodecCtx = nullptr;
        AVStream*        mVideoStream = nullptr;
        int              mVideoIndex = -1;
        
        AVFrame*  videoDstFrame = nullptr;
        uint8_t*  videoDstFrame_buff = nullptr;
        AVPacket* videoPkt = nullptr;
        int64_t   videoPktCount = 0;
        SwsContext* videoSwsCtx = nullptr;
        //视频处理end

        //音频处理start
        AVCodecContext* mAudioCodecCtx = nullptr;
        AVStream*       mAudioStream = nullptr;
        int             mAudioIndex = -1;

        AVFrame* audio_in_frame = nullptr;
        uint8_t* audio_in_frame_buff = nullptr;
        int      audio_in_nb_samples;
        AVFrame* audio_out_frame = nullptr;
        uint8_t* audio_out_frame_buff = nullptr;
        int      audio_out_nb_samples;
        AVPacket* audioPkt = nullptr;
        int64_t   audioPktCount = 0;
        SwrContext* audioSwrCtx = nullptr;
        //音频处理end

        std::queue<BXC_AvFrame*>  mVideoFrameQ;//待编码视频帧
        std::mutex                mVideoFrameQ_mtx;
        std::queue<BXC_AvPacket*> mVideoPacketQ;//编码后视频帧
        std::mutex                mVideoPacketQ_mtx;

        std::queue<BXC_AvFrame*>  mAudioFrameQ;//待编码音频帧
        std::mutex                mAudioFrameQ_mtx;
        std::queue<BXC_AvPacket*> mAudioPacketQ;//编码后音频帧
        std::mutex                mAudioPacketQ_mtx;
    };
}
#endif // FFMPEGENCODER_H
