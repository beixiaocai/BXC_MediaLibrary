#include "test.h"
#include <BXC_AudioRecorder.h>
#include <BXC_VideoRecorder.h>
#include <BXC_AvEncoder.h>
#include "Windows.h"
#include "FpsControl.h"
#include <iostream>
#include <thread>
using namespace BXC_MediaLibrary;


int test_AudioRecorder() {
    int fps = 20;
    int videoBitrate = 2000000;
    int width = 1920;
    int height = 1080;
    bool hasVideo = false;
    //    const char *videoCodecName = "h264_qsv";//qsv
    const char* videoCodecName = "libx264";//cpu
//    const char *videoCodecName = "h264_nvenc";//nvidia
//    const char *videoCodecName = "h264_amf";//amd
    BXC_PixelFormat pixelFormat = PIXEL_UNKNOWN;



    bool hasAudio = true;
    const char* audioCodecName = "libmp3lame";//mp3->decoders: mp3float,mp3 encoders: libmp3lame,libshine,mp3_mf
    //const char* audioCodecName = "aac";//aac->decoders: aac,aac_fixed encoders: aac,aac_mf
    int audioBitrate = 128000;

    const char* url = "test_AudioRecorder.mp4";
    url = nullptr;

    BXC_AudioRecorder* audioRecorder = new BXC_AudioRecorder("MICROPHONE");
    //audioRecorder->capture = "SOUNDCARD";
    //audioRecorder->capture = "MICROPHONE";

    int ret = BXC_AudioRecorder_Open(audioRecorder);
    if (ret < 0) {
        return -1;
    }

    printf("采样声道数=%d\n", audioRecorder->nb_channels);
    printf("采样声道存储比特数=%d\n", audioRecorder->nb_bits_sample);
    printf("采样率=%d\n", audioRecorder->sample_rate);
    printf("一帧音频的采样点数量=%d\n", audioRecorder->nb_samples);

    BXC_AvEncoder* avEncoder = new BXC_AvEncoder(hasVideo, videoCodecName, videoBitrate, pixelFormat,
        width, height, fps,
        hasAudio, audioCodecName, audioBitrate);

    ret = BXC_AvEncoder_Open(avEncoder, url);
    if (ret < 0) {
        return -1;
    }

    /* 1048576=1*1024*1024=1Mb*/
    BXC_AvFrame* audioFrame = new BXC_AvFrame(AVAUDIO, 10000);

    uint8_t* audioBuff = new uint8_t[10000];
    int      audioBuffSize = 0;

    int      recvBuffSize = 0;
    uint8_t* recvBuff = new uint8_t[10000];


    const int frameSize = 4608; // 4224=1056 * 4; 4608=1152 * 4;
    int64_t timestamp = 0;
    int64_t frameTimestamp = 0;
    int64_t frameCount = 0;

    while (true)
    {
        if (frameCount > 1000) {
            break;
        }
        ret = BXC_get_sample(audioRecorder, recvBuff, recvBuffSize, timestamp);
        if (ret >= 0) {
            if (recvBuffSize > 0) {
                if (0 == audioBuffSize) {
                    frameTimestamp = timestamp;
                }
                memcpy(audioBuff + audioBuffSize, recvBuff, recvBuffSize);
                audioBuffSize += recvBuffSize;

                if (audioBuffSize >= frameSize) {

                    memcpy(audioFrame->data, audioBuff, frameSize);
                    audioFrame->size = frameSize;
                    audioFrame->timestamp = frameTimestamp;
                    audioFrame->count = frameCount;

                    BXC_send_frame(avEncoder, audioFrame);
          
                    audioBuffSize -= frameSize;
                    memmove(audioBuff, audioBuff + frameSize, audioBuffSize);
                    frameTimestamp = timestamp;
                    ++frameCount;
                }
            }
        }else {
        }
    }
    
    if (recvBuff) {
        delete[]recvBuff;
        recvBuff = nullptr;
    }
    if (audioBuff) {
        delete[]audioBuff;
        audioBuff = nullptr;
    }

    BXC_AudioRecorder_Close(audioRecorder);
    delete audioRecorder;
    audioRecorder = nullptr;

    BXC_AvEncoder_Close(avEncoder);
    delete avEncoder;
    avEncoder = nullptr;



    return 0;
}
