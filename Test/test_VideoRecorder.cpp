#include "test.h"
#include <BXC_VideoRecorder.h>
#include <BXC_AvEncoder.h>
#include "FpsControl.h"
#include "Windows.h"
#include <iostream>
#include <thread>
#pragma warning( disable : 4996 )
using namespace BXC_MediaLibrary;

int test_VideoRecorder() {

    int fps = 25;
    int videoBitrate = 2000000;
    int width = 1920;
    int height = 1080;
    bool hasVideo = true;
    //    const char *videoCodecName = "h264_qsv";//qsv
    const char* videoCodecName = "libx264";//cpu
//    const char *videoCodecName = "h264_nvenc";//nvidia
//    const char *videoCodecName = "h264_amf";//amd

    const char* capture = "DXGI";
    //const char* capture = "imx188_azurewave(p)";
    const char* url = "test_VideoRecorder.mp4";
    //url = nullptr;

    int tempBuffSize = height * width * 4;
    uint8_t* tempBuff_bgra = new uint8_t[tempBuffSize];
    uint8_t* tempBuff_rgba = new uint8_t[tempBuffSize];
    int64_t frameStartTimestamp = 0;
    int64_t frameCount = 0;

    BXC_AvFrame* videoFrame = new BXC_AvFrame(AVVIDEO, tempBuffSize);

    BXC_VideoRecorder* videoRecorder = new BXC_VideoRecorder(capture,width,height,0);

    int ret = BXC_VideoRecorder_Open(videoRecorder);
    if (ret < 0) {
        return -1;
    }


    width = videoRecorder->factWidth;
    height = videoRecorder->factHeight;
    BXC_PixelFormat pixelFormat = videoRecorder->pixelFormat;

    BXC_AvEncoder* avEncoder = new BXC_AvEncoder(hasVideo, videoCodecName,videoBitrate, pixelFormat, 
        width, height, fps,
        false, nullptr,0);

    ret = BXC_AvEncoder_Open(avEncoder, url);
    if (ret < 0) {
        return -1;
    }

    BXC_AvPacket *videoPacket = new BXC_AvPacket(AVVIDEO, 600000);


    int recv;

    FpsControl fpsControl(fps);
    fpsControl.realTimeStart();

    while (true)
    {
        fpsControl.intervalStart();

        if (frameCount >= 1000) {
            break;
        }

        ret = BXC_get_frame(videoRecorder, tempBuff_bgra, tempBuffSize, frameStartTimestamp);
        //LOGI("%lld-%d,get_state=%d,tempBuffSize=%d,", frameStartTimestamp,frameCount, get_state, tempBuffSize);

        if (ret >= 0) {


            memcpy(videoFrame->data, tempBuff_bgra, tempBuffSize);
            videoFrame->size = tempBuffSize;
            videoFrame->timestamp = frameStartTimestamp;
            videoFrame->count = frameCount;


            BXC_send_frame(avEncoder, videoFrame);
            
            while (true)
            {
                recv = BXC_receive_packet(avEncoder,videoPacket);

                if (recv >= 0) {
                    printf("(videoPacket->size=%d) ", videoPacket->size);
                    for (int i = 0; i < 20; i++)
                    {
                        printf("%d-", videoPacket->data[i]);
                    }
                    printf("\n");
                }
                else {
                    break;
                }
            }
          


            ++frameCount;
            fpsControl.realTimeIncrease();
        }
        else {
            continue;
            //break;
        }

        fpsControl.adjust();


    }

    if (tempBuff_bgra) {
        delete[] tempBuff_bgra;
        tempBuff_bgra = nullptr;
    }
    if (tempBuff_rgba) {
        delete[] tempBuff_rgba;
        tempBuff_rgba = nullptr;
    }


    BXC_VideoRecorder_Close(videoRecorder);
    delete videoRecorder;
    videoRecorder = nullptr;

    BXC_AvEncoder_Close(avEncoder);
    delete avEncoder;
    avEncoder = nullptr;

    return 0;

}