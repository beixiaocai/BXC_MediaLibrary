#include "FFmpegRecorder.h"

#include "Common.h"
extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}
#pragma warning( disable : 4996 )

namespace BXC_MediaLibrary {
    FFmpegRecorder::FFmpegRecorder(const char* capture, int width, int height) : Intercept(capture,width, height) {
        mPixelFormat = PixelFormat::PIXEL_FMT_RGB;
        av_log_set_level(AV_LOG_QUIET);
    }
    FFmpegRecorder::~FFmpegRecorder() {
        this->release();
    }
    bool FFmpegRecorder::init(int idx) {
        
        //av_register_all();
        avdevice_register_all();
        std::string name = mCapture;
        std::string url = "video="+ name;
        const char* short_name = "dshow";

        /*
        //////gdigrab桌面截屏
        url = "desktop";
        short_name = "gdigrab";

        //////dshow桌面截屏
        url = "video=screen-capture-recorder";
        short_name = "dshow";

        //////linux桌面截屏
        url = ":1";
        short_name = "x11grab";

        */

        const AVInputFormat* inputFmt = av_find_input_format(short_name);


        AVDictionary* options = nullptr;
        //av_dict_set(&options, "list_devices", "true", 0);
        //av_dict_set(&options, "framerate", "25", 0);
        //av_dict_set(&options, "video_size", "1920x1080", 0);
#ifdef USE_DSHOW
        av_dict_set(&options, "pixel_format", "yuv420p", 0);
#endif


        if (0 != avformat_open_input(&mFmtCtx, url.data(), inputFmt, &options)) {
            LOGE("avformat_open_input error");
            avformat_close_input(&mFmtCtx);
            return false;
        }
        if (0 != avformat_find_stream_info(mFmtCtx, nullptr)) {
            LOGE("avformat_find_stream_info error");
            return false;
        }
        for (int i = 0; i < mFmtCtx->nb_streams; i++) {
            if (mFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                mIndex = i;
                mStream = mFmtCtx->streams[mIndex];
                break;
            }
        }
        if (!mStream) {
            LOGE("stream error");
            return false;
        }

        const AVCodec* codec = avcodec_find_decoder(mStream->codecpar->codec_id);//AV_CODEC_ID_MJPEG
        if (!codec) {
            LOGE("avcodec_find_decoder error");
            return false;
        }
        mCodecCtx = avcodec_alloc_context3(codec);
        if (0 != avcodec_parameters_to_context(mCodecCtx, mStream->codecpar)) {
            LOGE("avcodec_parameters_to_context error");
            return false;
        }

        mFactWidth = mCodecCtx->width;
        mFactHeight = mCodecCtx->height;
        
        if (avcodec_open2(mCodecCtx, codec, nullptr) < 0)
        {
            LOGE("avcodec_open2 error");
            return false;
        }
        //初始化一些读取数据时必备参数

        mPkt = av_packet_alloc();
        srcFrame = av_frame_alloc();

        dstFrame = av_frame_alloc();
        AVPixelFormat dstFormat = AV_PIX_FMT_RGB24;
        dstFrame->format = dstFormat;

        dstFrame->width = mFactWidth;
        dstFrame->height = mFactHeight;
        int dstFrame_buff_size = av_image_get_buffer_size(dstFormat, dstFrame->width, dstFrame->height, 1);
        dstFrame_buff = (uint8_t*)av_malloc(dstFrame_buff_size);
        av_image_fill_arrays(dstFrame->data, dstFrame->linesize, dstFrame_buff, dstFormat, dstFrame->width, dstFrame->height, 1);

        dstDataSize = dstFrame->width * dstFrame->height * 3;

        return true;
    }

    void FFmpegRecorder::release() {
        if (mFmtCtx) {
            avformat_close_input(&mFmtCtx);
            avformat_free_context(mFmtCtx);
            mFmtCtx = nullptr;
        }

        if (mCodecCtx) {
            avcodec_close(mCodecCtx);
            avcodec_free_context(&mCodecCtx);
            mCodecCtx = nullptr;
            mIndex = -1;
        }
        if (mPkt) {
            av_packet_unref(mPkt);
            av_packet_free(&mPkt);
            mPkt = nullptr;
        }
        if (srcFrame) {
            av_frame_free(&srcFrame);
            srcFrame = nullptr;
        }

        if (dstFrame_buff) {
            av_free(dstFrame_buff);
            dstFrame_buff = nullptr;
        }
        if (dstFrame) {
            av_frame_free(&dstFrame);
            dstFrame = nullptr;
        }

    }


    bool FFmpegRecorder::getFrame(uint8_t* buffer, int& size, int64_t& timestamp) {
        bool get_state = false;
        //mCodecCtx->pix_fmt;//AV_PIX_FMT_YUVJ422P
        
        size = dstDataSize;
        timestamp = getCurTimestamp();
        int ret = av_read_frame(mFmtCtx, mPkt);

        if (0 == ret) {
            //LOGI("av_read_frame mPkt->size=%d", mPkt->size);
            avcodec_send_packet(mCodecCtx, mPkt);
            while (true) {
                ret = avcodec_receive_frame(mCodecCtx, srcFrame);

                if (ret >= 0) {
                    auto img_convert_ctx = sws_getContext(srcFrame->width, srcFrame->height, mCodecCtx->pix_fmt,
                        dstFrame->width, dstFrame->height, (enum AVPixelFormat)dstFrame->format, SWS_BICUBIC, NULL, NULL, NULL);

                    sws_scale(img_convert_ctx, (const unsigned char* const*)srcFrame->data,
                        srcFrame->linesize, 0, srcFrame->height,
                        dstFrame->data, dstFrame->linesize);
                    sws_freeContext(img_convert_ctx);

                    //LOGI("avcodec_receive_frame dstFrame->linesize[0]=%d", dstFrame->linesize[0]);
                    memcpy(buffer, dstFrame->data[0], dstDataSize);
                    get_state = true;
                }
                else {
                    //LOGE("ret=%d", ret);
                    break;
                }

            }
            av_packet_unref(mPkt);
        }
        else {
            LOGE("av_read_frame error,ret=%d", ret);
        }

        return get_state;
    }
}