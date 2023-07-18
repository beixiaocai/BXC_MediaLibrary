#include "FFmpegEncoder.h"
#include "../include/BXC_AvFrame.h"
#include "Common.h"
extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/time.h>
}
#pragma warning( disable : 4996 )

namespace BXC_MediaLibrary {
    FFmpegEncoder::FFmpegEncoder()
    {
        av_log_set_level(AV_LOG_ERROR);
    }

    FFmpegEncoder::~FFmpegEncoder() {
        //需要先调用stop()，才能够delete进行析构资源

        clearVideoFrame();
        clearAudioFrame();
        clearVideoPacket();
        clearAudioPacket();


        if (mHasVideo) {
            if (videoDstFrame_buff) {
                av_free(videoDstFrame_buff);
                videoDstFrame_buff = nullptr;
            }
            if (videoDstFrame) {
                av_frame_free(&videoDstFrame);
                videoDstFrame = nullptr;
            }
            if (videoPkt) {
                av_packet_free(&videoPkt);
                videoPkt = nullptr;
            }
            if (mVideoCodecCtx) {
                avcodec_close(mVideoCodecCtx);
                avcodec_free_context(&mVideoCodecCtx);
                mVideoCodecCtx = nullptr;
                mVideoIndex = -1;
            }
        }
        if (mHasAudio) {
            if (audio_in_frame_buff) {
                av_free(audio_in_frame_buff);
                audio_in_frame_buff = nullptr;
            }
            if (audio_in_frame) {
                av_frame_free(&audio_in_frame);
                audio_in_frame = nullptr;
            }

            if (audio_out_frame_buff) {
                av_free(audio_out_frame_buff);
                audio_out_frame_buff = nullptr;
            }

            if (audio_out_frame) {
                av_frame_free(&audio_out_frame);
                audio_out_frame = nullptr;
            }
            if (audioPkt) {
                av_packet_free(&audioPkt);
                audioPkt = nullptr;
            }
            if (audioSwrCtx) {
                swr_free(&audioSwrCtx);
                audioSwrCtx = nullptr;
            }
            if (mAudioCodecCtx) {
                avcodec_close(mAudioCodecCtx);
                avcodec_free_context(&mAudioCodecCtx);
                mAudioCodecCtx = nullptr;
                mAudioIndex = -1;
            }
        }
        
        if (mContainWrite) {
            if (mFmtCtx) {

                if (mFmtCtx && !(mFmtCtx->oformat->flags & AVFMT_NOFILE)) { //写文件或者推流需要关闭
                    avio_close(mFmtCtx->pb);
                }
                avformat_free_context(mFmtCtx);
                mFmtCtx = nullptr;
            }
        }
    }

    bool FFmpegEncoder::start(BXC_AvEncoder* encoder, const char* url) {
        if (url) {
            mContainWrite = true;
        }

        mHasVideo = encoder->hasVideo;
        mHasAudio = encoder->hasAudio;
        mVideoSrcFps = encoder->fps;
        mVideoSrcWidth = encoder->width;
        mVideoSrcHeight = encoder->height;

        if (mHasVideo) {
            const AVCodec* videoCodec = avcodec_find_encoder_by_name(encoder->videoCodecName);
            if (!videoCodec) {
                LOGE("avcodec_find_encoder_by_name error,videoCodecName=%s", encoder->videoCodecName);
                return false;
            }
            mVideoCodecCtx = avcodec_alloc_context3(videoCodec);
            if (!mVideoCodecCtx) {
                LOGE("avcodec_alloc_context3 error,videoCodecName=%s", encoder->videoCodecName);
                return false;
            }
            // CBR: 固定比特率
            //    oVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
            //    oVideoCodecCtx->bit_rate = videoBitrate;
            //    oVideoCodecCtx->rc_min_rate = videoBitrate;
            //    oVideoCodecCtx->rc_max_rate = videoBitrate;
            //    oVideoCodecCtx->bit_rate_tolerance = videoBitrate;
            //VBR
            mVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
            mVideoCodecCtx->rc_min_rate = encoder->videoBitrate / 2;
            mVideoCodecCtx->rc_max_rate = encoder->videoBitrate / 2 + encoder->videoBitrate;
            mVideoCodecCtx->bit_rate = encoder->videoBitrate;
            //ABR: 平均码率
            //oVideoCodecCtx->bit_rate = videoBitrate;

            mVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
            mVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;// AV_PIX_FMT_NV12，AV_PIX_FMT_YUV420P
            mVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            mVideoCodecCtx->time_base = { 1, encoder->fps };
            mVideoCodecCtx->framerate = { encoder->fps, 1 };
            mVideoCodecCtx->width = mVideoSrcWidth;
            mVideoCodecCtx->height = mVideoSrcHeight;
            mVideoCodecCtx->keyint_min = encoder->fps;
            mVideoCodecCtx->thread_count = 1;
            //        mVideoCodecCtx->max_b_frames = 3;

            if (AV_CODEC_ID_H264 == mVideoCodecCtx->codec_id) {
                av_opt_set(mVideoCodecCtx->priv_data, "profile", "main", 0);
                av_opt_set(mVideoCodecCtx->priv_data, "b-pyramid", "none", 0);
                av_opt_set(mVideoCodecCtx->priv_data, "preset", "superfast", 0);
                av_opt_set(mVideoCodecCtx->priv_data, "tune", "zerolatency", 0);

            }
            else if (AV_CODEC_ID_H265 == mVideoCodecCtx->codec_id) {

            }
            if (avcodec_open2(mVideoCodecCtx, videoCodec, nullptr) < 0) {// 将编码器上下文和编码器进行关联
                LOGE("avcodec_open2 error,videoCodecName=%s", encoder->videoCodecName);
                return false;
            }


        }
        if (mHasAudio) {
            const AVCodec* audioCodec = avcodec_find_encoder_by_name(encoder->audioCodecName);//avcodec_find_encoder(AV_CODEC_ID_AAC);
            if (!audioCodec) {
                LOGE("avcodec_find_encoder_by_name error,audioCodecName=%s", encoder->audioCodecName);
                return false;
            }
            int audio_sample_rate;
            int audio_nb_samples;
            if (AV_CODEC_ID_MP3 == audioCodec->id) {
                audio_sample_rate = 48000;
                audio_nb_samples = 1056;
            }
            else if (AV_CODEC_ID_AAC == audioCodec->id) {
                audio_sample_rate = 44100;
                audio_nb_samples = 1024;
            }
            else {
                LOGE("unsupported audio codec,audioCodecName=%s", encoder->audioCodecName);
                return false;
            }
            //音频帧频率 = audio_sample_rate / audio_nb_samples;

            mAudioCodecCtx = avcodec_alloc_context3(audioCodec);
            if (!mAudioCodecCtx) {
                LOGE("avcodec_alloc_context3 error,audioCodecName=%s", encoder->audioCodecName);
                return false;
            }
            mAudioCodecCtx->codec_id = audioCodec->id;
            mAudioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
            mAudioCodecCtx->bit_rate = encoder->audioBitrate;//音频码率
            mAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;// 声道层
            mAudioCodecCtx->channels = av_get_channel_layout_nb_channels(mAudioCodecCtx->channel_layout);// 声道数
            mAudioCodecCtx->sample_rate = audio_sample_rate;//采样率
            mAudioCodecCtx->frame_size = audio_nb_samples;//每帧单个通道的采样点数
            mAudioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;//ffmpeg对于音频编码格式默认只支持AV_SAMPLE_FMT_FLTP
            mAudioCodecCtx->time_base = { audio_nb_samples, audio_sample_rate };
            mAudioCodecCtx->framerate = { audio_sample_rate, audio_nb_samples };

            if (AV_CODEC_ID_AAC == audioCodec->id) {
                mAudioCodecCtx->profile = FF_PROFILE_AAC_LOW;
            }

            if (avcodec_open2(mAudioCodecCtx, audioCodec, NULL) < 0) {// 将编码器上下文和编码器进行关联
                LOGE("avcodec_open2 error,audioCodecName=%s", encoder->audioCodecName);
                return false;
            }
        }
        
        if (mContainWrite) {
            if (avformat_alloc_output_context2(&mFmtCtx, nullptr, nullptr, url) < 0) {
                LOGE("avformat_alloc_output_context2 error");
                return false;
            }
            // av_dump_format(mFmtCtx, 0, url, 1);
            if (mHasVideo) {
                //        av_opt_set(mVideoCodecCtx->priv_data, "crf", "18", AV_OPT_SEARCH_CHILDREN);
                mVideoStream = avformat_new_stream(mFmtCtx, mVideoCodecCtx->codec);
                if (!mVideoStream)
                {
                    LOGE("avformat_new_stream error,videoCodecName=%s", encoder->videoCodecName);
                    return false;
                }
                mVideoStream->id = mFmtCtx->nb_streams - 1;
                mVideoIndex = mVideoStream->index;
                avcodec_parameters_from_context(mVideoStream->codecpar, mVideoCodecCtx);

                mFmtCtx->video_codec_id = mFmtCtx->oformat->video_codec;
                if (mFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                    mVideoCodecCtx->flags = mVideoCodecCtx->flags | AV_CODEC_FLAG_GLOBAL_HEADER;
                }

            }
            if (mHasAudio) {
                mAudioStream = avformat_new_stream(mFmtCtx, mAudioCodecCtx->codec);
                if (!mAudioStream) {
                    LOGE("avformat_new_stream error,audioCodecName=%s", encoder->audioCodecName);
                    return false;
                }
                mAudioStream->id = mFmtCtx->nb_streams - 1;
                mAudioIndex = mAudioStream->id;
                avcodec_parameters_from_context(mAudioStream->codecpar, mAudioCodecCtx);

                mFmtCtx->audio_codec_id = mFmtCtx->oformat->audio_codec;
            }
            if (!(mFmtCtx->oformat->flags & AVFMT_NOFILE)) {
                if (avio_open(&mFmtCtx->pb, url, AVIO_FLAG_WRITE) < 0) {
                    LOGE("avio_open error");
                    return false;
                }
            }
        }


        if (mHasVideo) {

            //输入参数start
            AVPixelFormat videoSrcFormat;
            if (PIXEL_FMT_BGRA == encoder->pixelFormat) {
                videoSrcFormat = AV_PIX_FMT_BGRA;
                mVideoSrcChannel = 4;
            }
            else if (PIXEL_FMT_RGB == encoder->pixelFormat) {
                videoSrcFormat = AV_PIX_FMT_RGB24;
                mVideoSrcChannel = 3;
            }
            else {
                LOGE("unsupported video pixelFormat,pixelFormat=%d", encoder->pixelFormat);
                return false;
            }
            mVideoSrcStride[0] = mVideoSrcChannel * mVideoSrcWidth;
            //输入参数end

            //输出参数start
            int videoDstWidth = mVideoCodecCtx->width;
            int videoDstHeight = mVideoCodecCtx->height;
            AVPixelFormat videoDstFormat = mVideoCodecCtx->pix_fmt;//AV_PIX_FMT_YUV420P;
            videoDstFrame = av_frame_alloc();
            videoDstFrame->format = videoDstFormat;
            videoDstFrame->width = videoDstWidth;
            videoDstFrame->height = videoDstHeight;

            int videoDstFrame_buff_size = av_image_get_buffer_size(videoDstFormat, videoDstWidth, videoDstHeight, 1);
            uint8_t* videoDstFrame_buff = (uint8_t*)av_malloc(videoDstFrame_buff_size);
            av_image_fill_arrays(videoDstFrame->data, videoDstFrame->linesize, videoDstFrame_buff, videoDstFormat, videoDstWidth, videoDstHeight, 1);

            //输出参数end


            videoSwsCtx = sws_getCachedContext(videoSwsCtx,
                mVideoSrcWidth, mVideoSrcHeight, videoSrcFormat,
                videoDstWidth, videoDstHeight, videoDstFormat,
                0, 0, 0, 0);
            videoPkt = av_packet_alloc();

        }
        if (mHasAudio) {
            // 输入参数start
            uint64_t audio_in_channel_layout = AV_CH_LAYOUT_STEREO;// 输入声道层
            int audio_in_channels = av_get_channel_layout_nb_channels(audio_in_channel_layout);// 输入声道数
            AVSampleFormat audio_in_sample_fmt = AV_SAMPLE_FMT_S16;
            int audio_in_sample_rate = 48000;
            audio_in_nb_samples = 1152;//1024,1056,1152;

            audio_in_frame = av_frame_alloc();
            audio_in_frame->channels = audio_in_channels;
            audio_in_frame->format = audio_in_sample_fmt;
            audio_in_frame->nb_samples = audio_in_nb_samples;

            int audio_in_frame_buff_size = av_samples_get_buffer_size(NULL, audio_in_channels, audio_in_nb_samples, audio_in_sample_fmt, 1);
            audio_in_frame_buff = (uint8_t*)av_malloc(audio_in_frame_buff_size);
            avcodec_fill_audio_frame(audio_in_frame, audio_in_channels, audio_in_sample_fmt, (const uint8_t*)audio_in_frame_buff, audio_in_frame_buff_size, 1);
            // 输入参数end

            // 输出参数start
            uint64_t audio_out_channel_layout = mAudioCodecCtx->channel_layout;
            int audio_out_channels = mAudioCodecCtx->channels;// 输出声道数
            AVSampleFormat audio_out_sample_fmt = mAudioCodecCtx->sample_fmt;
            int audio_out_sample_rate = mAudioCodecCtx->sample_rate;
            audio_out_nb_samples = mAudioCodecCtx->frame_size;//1024,1056,

            audio_out_frame = av_frame_alloc();
            audio_out_frame->channels = audio_out_channels;
            audio_out_frame->format = audio_out_sample_fmt;
            audio_out_frame->nb_samples = audio_out_nb_samples;

            int audio_out_frame_buff_size = av_samples_get_buffer_size(NULL, audio_out_channels, audio_out_nb_samples, audio_out_sample_fmt, 1);
            audio_out_frame_buff = (uint8_t*)av_malloc(audio_out_frame_buff_size);
            avcodec_fill_audio_frame(audio_out_frame, audio_out_channels, audio_out_sample_fmt, (const uint8_t*)audio_out_frame_buff, audio_out_frame_buff_size, 1);
            // 输出参数end
        //    int audio_in_sample_size  = audio_in_nb_samples * av_get_bytes_per_sample(audio_in_sample_fmt) * audio_in_channels;//读取一帧音频字节长度
        //    av_new_packet(audioPkt, audio_out_frame_buff_size);
        //    audioPkt->data = nullptr;
        //    audioPkt->size = 0;

            audioSwrCtx = swr_alloc();
            swr_alloc_set_opts(audioSwrCtx,
                audio_out_channel_layout,
                audio_out_sample_fmt,
                audio_out_sample_rate,
                audio_in_channel_layout,
                audio_in_sample_fmt,
                audio_in_sample_rate,
                0, nullptr);
            swr_init(audioSwrCtx);
            audioPkt = av_packet_alloc();
        }

        //初始化完成，开启线程
        mIsStop = false;
        mThread = new std::thread(FFmpegEncoder::startThread, this);
        return true;
    }
    
    bool FFmpegEncoder::stop() {
        mIsStop = true;
        if (mThread) {
            mThread->join();
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            delete mThread;
            mThread = nullptr;
        }

        return true;
    }
    int64_t FFmpegEncoder::gettime() {
        return av_gettime();
    }

    void FFmpegEncoder::startThread(void* arg) {
        FFmpegEncoder* encoder = (FFmpegEncoder*)arg;
        encoder->handle();
    }
    bool FFmpegEncoder::handle() {
        if (mContainWrite) {
            AVDictionary* options = nullptr;
            if (avformat_write_header(mFmtCtx, &options) < 0) {//写文件头
                LOGE("avformat_write_header error");
                return false;
            }
        }

        BXC_AvFrame* videoFrame = nullptr;
        BXC_AvFrame* audioFrame = nullptr;

        while (true) {
            if (mIsStop) {
                break;
            }
            if (mHasVideo && !videoFrame) {
                 videoFrame = getVideoFrame();
            }
            if (mHasAudio && !audioFrame) {
                 audioFrame = getAudioFrame();
            }
            if (mHasVideo && !mHasAudio) {//只处理视频
                if (videoFrame) {
                    encodeVideoFrame(videoFrame);
                    delete videoFrame;
                    videoFrame = nullptr;
                }
            }
            else if (!mHasVideo && mHasAudio) {//只处理音频
                if (audioFrame) {
                    encodeAudioFrame(audioFrame);
                    delete audioFrame;
                    audioFrame = nullptr;
                }
            }
            else if (mHasVideo && mHasAudio) {//同时处理音频和视频

                if (videoFrame && audioFrame) {
                    if (videoFrame->timestamp < audioFrame->timestamp) {
                        encodeVideoFrame(videoFrame);
                        delete videoFrame;
                        videoFrame = nullptr;
                    }
                    else {
                        encodeAudioFrame(audioFrame);
                        delete audioFrame;
                        audioFrame = nullptr;
                    }
                }
                else if (videoFrame && !audioFrame) {
                    encodeVideoFrame(videoFrame);
                    delete videoFrame;
                    videoFrame = nullptr;
                }
                else if (!videoFrame && audioFrame) {
                    encodeAudioFrame(audioFrame);
                    delete audioFrame;
                    audioFrame = nullptr;
                }

            }
        }

        int rc = 0;
        int videoRet, audioRet;
        while (true) {
            if (mHasVideo) {
                videoRet = avcodec_receive_packet(mVideoCodecCtx, videoPkt);
                if (videoRet >= 0) {
                    videoPkt->stream_index = mVideoStream->index;
                    writePkt(videoPkt);
                    ++videoPktCount;
                    av_packet_unref(videoPkt);
                }
            }
            else {
                videoRet = -1;
            }
            if (mHasAudio) {
                audioRet = avcodec_receive_packet(mAudioCodecCtx, audioPkt);
                if (audioRet >= 0) {
                    audioPkt->stream_index = mAudioStream->index;
                    writePkt(audioPkt);
                    ++audioPktCount;
                    av_packet_unref(audioPkt);
                }
            }
            else {
                audioRet = -1;
            }
            if (videoRet < 0 && audioRet < 0) {
                break;
            }
            ++rc;
        }
        
        if (mContainWrite) {
            if (av_write_trailer(mFmtCtx) < 0) {//写文件尾
                LOGE("av_write_trailer error");
            }
        }
        return true;
    }
    bool FFmpegEncoder::encodeVideoFrame(BXC_AvFrame* videoFrame) {
        sws_scale(videoSwsCtx, (const uint8_t* const*)&videoFrame->data, mVideoSrcStride, 0, mVideoSrcHeight,
            videoDstFrame->data, videoDstFrame->linesize);

        if (mContainWrite) {
            videoDstFrame->pkt_dts = videoDstFrame->pts = av_rescale_q_rnd(videoPktCount, mVideoCodecCtx->time_base, mVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            videoDstFrame->pkt_duration = av_rescale_q_rnd(1, mVideoCodecCtx->time_base, mVideoStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            videoDstFrame->pkt_pos = -1;
        }
        int ret, rc = 0;
        avcodec_send_frame(mVideoCodecCtx, videoDstFrame);

        while (true)
        {
            ret = avcodec_receive_packet(mVideoCodecCtx, videoPkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                //LOGE("video avcodec_receive_packet error,EAGAIN or EOF ret=%d", ret);
                break;
            }
            else if (ret < 0) {
                LOGE("video avcodec_receive_packet error,ret=%d",ret);
                break;
            }
            else {
                ++videoPktCount;
                ++rc;

                if (mContainWrite) {
                    videoPkt->stream_index = mVideoStream->index;
                    writePkt(videoPkt);
                }
                else {
                    int64_t timestamp = getCurTimestamp();
                    BXC_AvPacket* packet = new BXC_AvPacket(
                        AVVIDEO,
                        videoPkt->size,
                        videoPkt->stream_index,
                        videoPkt->pts,
                        videoPkt->dts,
                        videoPkt->duration,
                        videoPkt->pos,
                        timestamp,
                        videoPktCount);
                    memcpy(packet->data, videoPkt->data, videoPkt->size);
                    sendPacket(packet);
                }
                //LOGI("videoPktCount=%lld,rc=%d,pts=%lld,pos=%lld,size=%d", videoPktCount, rc, videoPkt->pts,videoPkt->pos,videoPkt->size);

                av_packet_unref(videoPkt);
            }
        }
        return true;
    }
    bool FFmpegEncoder::encodeAudioFrame(BXC_AvFrame* audioFrame) {

        memcpy(audio_in_frame_buff, audioFrame->data, audioFrame->size);
        swr_convert(audioSwrCtx, audio_out_frame->data, audio_out_nb_samples, (const uint8_t**)audio_in_frame->data, audio_in_nb_samples);
        if (mContainWrite) {
            audio_out_frame->pts = audio_out_frame->pkt_dts = av_rescale_q_rnd(audioPktCount, mAudioCodecCtx->time_base, mAudioStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            audio_out_frame->pkt_duration = av_rescale_q_rnd(1, mAudioCodecCtx->time_base, mAudioStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            audio_out_frame->pkt_pos = -1;
        }
        int ret, rc = 0;

        avcodec_send_frame(mAudioCodecCtx, audio_out_frame);

        while (true)
        {
            ret = avcodec_receive_packet(mAudioCodecCtx, audioPkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                //LOGE("audio avcodec_receive_packet error,EAGAIN or EOF ret=%d", ret);
                break;
            }
            else if (ret < 0) {
                LOGE("audio avcodec_receive_packet error,ret=%d", ret);
                break;
            }
            else {
                ++audioPktCount;
                ++rc;

                if (mContainWrite) {
                    audioPkt->stream_index = mAudioStream->index;
                    writePkt(audioPkt);
                }
                else {
                    int64_t timestamp = getCurTimestamp();
                    BXC_AvPacket* packet = new BXC_AvPacket(
                        AVAUDIO,
                        audioPkt->size,
                        audioPkt->stream_index,
                        audioPkt->pts,
                        audioPkt->dts,
                        audioPkt->duration,
                        audioPkt->pos,
                        timestamp,
                        audioPktCount);
                    memcpy(packet->data, audioPkt->data, audioPkt->size);
                    sendPacket(packet);
                
                }
                //LOGI("audioPktCount=%lld,rc=%d,pts=%lld,pos=%lld,size=%d", audioPktCount, rc, audioPkt->pts, audioPkt->pos, audioPkt->size);
                av_packet_unref(audioPkt);
            }
        }
        return true;
    }

    void FFmpegEncoder::writePkt(AVPacket* pkt) {
        //av_interleaved_write_frame(mFmtCtx, pkt);
        av_write_frame(mFmtCtx, pkt);
    }
    bool FFmpegEncoder::getHasVideo() {
        return mHasVideo;
    }
    bool FFmpegEncoder::getHasAudio() {
        return mHasAudio;
    }

    void FFmpegEncoder::sendFrame(BXC_AvFrame* frame) {
        if (AVVIDEO == frame->type) {
            mVideoFrameQ_mtx.lock();
            mVideoFrameQ.push(frame);
            mVideoFrameQ_mtx.unlock();
        }
        else if (AVAUDIO == frame->type) {
            mAudioFrameQ_mtx.lock();
            mAudioFrameQ.push(frame);
            mAudioFrameQ_mtx.unlock();
        }

    }
    BXC_AvFrame* FFmpegEncoder::getVideoFrame() {
        BXC_AvFrame* videoFrame = nullptr;

        mVideoFrameQ_mtx.lock();
        if (!mVideoFrameQ.empty()) {
            videoFrame = mVideoFrameQ.front();
            mVideoFrameQ.pop();
            mVideoFrameQ_mtx.unlock();
        }
        else {
            mVideoFrameQ_mtx.unlock();
        }
        return videoFrame;

    }
    BXC_AvFrame* FFmpegEncoder::getAudioFrame() {
        BXC_AvFrame* audioFrame = nullptr;

        mAudioFrameQ_mtx.lock();
        if (!mAudioFrameQ.empty()) {
            audioFrame = mAudioFrameQ.front();
            mAudioFrameQ.pop();
            mAudioFrameQ_mtx.unlock();
        }
        else {
            mAudioFrameQ_mtx.unlock();
        }
        return audioFrame;
    }
    void FFmpegEncoder::sendPacket(BXC_AvPacket* packet) {
        if (AVVIDEO == packet->type) {
            mVideoPacketQ_mtx.lock();
            mVideoPacketQ.push(packet);
            mVideoPacketQ_mtx.unlock();
        }
        else if (AVAUDIO == packet->type) {
            mAudioPacketQ_mtx.lock();
            mAudioPacketQ.push(packet);
            mAudioPacketQ_mtx.unlock();
        }
    }

    BXC_AvPacket* FFmpegEncoder::getVideoPacket() {
        BXC_AvPacket* videoPacket = nullptr;
        mVideoPacketQ_mtx.lock();
        if (mVideoPacketQ.empty()) {
            mVideoPacketQ_mtx.unlock();
        }
        else {
            videoPacket = mVideoPacketQ.front();
            mVideoPacketQ.pop();
            mVideoPacketQ_mtx.unlock();
        }
        return videoPacket;
    }
    BXC_AvPacket* FFmpegEncoder::getAudioPacket() {
        BXC_AvPacket* audioPacket = nullptr;
        mAudioPacketQ_mtx.lock();
        if (mAudioPacketQ.empty()) {
            mAudioPacketQ_mtx.unlock();
        }
        else {
            audioPacket = mAudioPacketQ.front();
            mAudioPacketQ.pop();
            mAudioPacketQ_mtx.unlock();
        }
        return audioPacket;
    }

    void FFmpegEncoder::clearVideoFrame() {
        mVideoFrameQ_mtx.lock();
        while (true) {
            if (!mVideoFrameQ.empty()) {
                BXC_AvFrame* obj = mVideoFrameQ.front();
                mVideoFrameQ.pop();
                delete obj;
                obj = nullptr;
            }
            else {
                break;
            }
        }
        mVideoFrameQ_mtx.unlock();
    };
    void FFmpegEncoder::clearAudioFrame() {
        mAudioFrameQ_mtx.lock();
        while (true) {
            if (!mAudioFrameQ.empty()) {
                BXC_AvFrame* obj = mAudioFrameQ.front();
                mAudioFrameQ.pop();
                delete obj;
                obj = nullptr;
            }
            else {
                break;
            }
        }
        mAudioFrameQ_mtx.unlock();

    }
    void FFmpegEncoder::clearVideoPacket() {
        mVideoPacketQ_mtx.lock();
        while (true) {
            if (!mVideoPacketQ.empty()) {
                BXC_AvPacket* obj = mVideoPacketQ.front();
                mVideoPacketQ.pop();
                delete obj;
                obj = nullptr;
            }
            else {
                break;
            }
        }
        mVideoPacketQ_mtx.unlock();
    }
    void FFmpegEncoder::clearAudioPacket() {
        mAudioPacketQ_mtx.lock();
        while (true) {
            if (!mAudioPacketQ.empty()) {
                BXC_AvPacket* obj = mAudioPacketQ.front();
                mAudioPacketQ.pop();
                delete obj;
                obj = nullptr;
            }
            else {
                break;
            }
        }
        mAudioPacketQ_mtx.unlock();
    }

};