#ifndef WINDOWSAUDIORECORDER_H
#define WINDOWSAUDIORECORDER_H
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <windowsx.h>
#include <avrt.h>
#include <conio.h>
#include <string>
#pragma comment(lib, "Avrt.lib")

namespace BXC_MediaLibrary {
    class WindowsAudioRecorder
    {
    public:
        enum {
            SOUNDCARD = 10, //声卡（扬声器）
            MICROPHONE    //麦克风
        };
        WindowsAudioRecorder(int captureType);
        ~WindowsAudioRecorder();

    public:

        int get_sample(unsigned char*& buffer, int& size, int64_t& timestamp);//获取采样点
        int get_nb_channels();   //获取采样声道数（常用值：2,1）
        int get_nb_bits_sample();//获取采样时每一个采样点的存储比特数（常用值：16,8,24）
        int get_sample_rate();   //获取采样率（常用值：44100,48000）
        int get_nb_samples();    //获取采样时单声道一帧音频的采样点数量（常用值：44100->1024,48000->1056）

    private:
        bool reInit();

        bool init();
        void release();

        int  mCaptureType = 0;
        bool mInited = false;
        IMMDevice* pDevice;
        WAVEFORMATEX* mWaveFmt;
        IAudioClient*        pAudioClient = nullptr;
        IAudioCaptureClient* pAudioCaptureClient = nullptr;
        HANDLE mTask = nullptr;

        UINT nb_samples = 0;//每一帧音频的采样点数量
        //REFERENCE_TIME frame_time;//一帧音频的间隔时长（单位毫秒）
    private:
        IMMDevice* GetDefaultDevice();
        bool AdjustFormat(WAVEFORMATEX* pwfx);
    };
}

#endif //WINDOWSAUDIORECORDER_H