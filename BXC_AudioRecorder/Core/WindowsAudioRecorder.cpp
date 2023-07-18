#include "WindowsAudioRecorder.h"
#include "Common.h"


namespace BXC_MediaLibrary {
    WindowsAudioRecorder::WindowsAudioRecorder(int captureType){
        mCaptureType = captureType;
        this->init();
    }
    WindowsAudioRecorder::~WindowsAudioRecorder(){
        this->release();
    }

    bool WindowsAudioRecorder::init() {
        
        
        pDevice = GetDefaultDevice();

        if (!pDevice) {
            return false;
        }
        
        HRESULT hr = 0;
        DWORD   taskIndex = 0;
        do
        {
            hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
            if (FAILED(hr))
                break;

            hr = pAudioClient->GetMixFormat(&mWaveFmt);

            if (FAILED(hr))
                break;
            AdjustFormat(mWaveFmt);
            if (SOUNDCARD == mCaptureType) {
                hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, mWaveFmt, 0);
            }
            else {
                hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, mWaveFmt, 0);
            }
            if (FAILED(hr))
                break;

            hr = pAudioClient->GetBufferSize(&nb_samples);

            if (FAILED(hr))
                break;

            hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pAudioCaptureClient);

            if (FAILED(hr))
                break;

            //frame_time = 1000 * nb_samples / mWaveFmt->nSamplesPerSec;

            if (SOUNDCARD == mCaptureType) {
                mTask = AvSetMmThreadCharacteristics(L"Capture", &taskIndex);
            }
            else {
                mTask = AvSetMmThreadCharacteristics(L"Audio", &taskIndex);
            }
            if (!mTask)
                break;

            hr = pAudioClient->Start();
            if (FAILED(hr))
                break;

            mInited = true;
        } while (0);
    
        return true;
    }
    void WindowsAudioRecorder::release() {
        if (mTask)
        {
            AvRevertMmThreadCharacteristics(mTask);
            mTask = nullptr;
        }

        if (pAudioCaptureClient)
        {
            pAudioCaptureClient->Release();
            pAudioCaptureClient = nullptr;
        }

        if (mWaveFmt)
        {
            CoTaskMemFree(mWaveFmt);
            mWaveFmt = nullptr;
        }

        if (pAudioClient)
        {
            if (mInited) {
                pAudioClient->Stop();
            }
            
            pAudioClient->Release();
            pAudioClient = nullptr;
        }

        if (pDevice)
        {
            pDevice->Release();
            pDevice = nullptr;
        }

        mInited = false;
    }
    bool WindowsAudioRecorder::reInit() {
        this->release();
        return this->init();
    }
    int WindowsAudioRecorder::get_sample(unsigned char*& buffer, int& size, int64_t& timestamp) {
        if (!mInited) {
            LOGE("get_sample mInited error");
            return -2;
        }

        HRESULT hr = 0;
        UINT32 nextPacketSize = 0;
        UINT32 numFramesToRead;
        unsigned char* pData = nullptr;
        
        DWORD  dwFlags;
        //Sleep(frame_time / 2);
        size = 0;
        timestamp = getCurTimestamp();
        int i = 0;

        while (true)
        {
            hr = pAudioCaptureClient->GetNextPacketSize(&nextPacketSize);
            if (FAILED(hr)) {
                LOGE("get_sample GetNextPacketSize error:hr=%d", hr);
                return -3;
            }

            if (nextPacketSize != 0) {
                hr = pAudioCaptureClient->GetBuffer(
                    &pData,
                    &numFramesToRead,
                    &dwFlags,
                    nullptr,
                    nullptr);

                if (FAILED(hr)) {
                    //录制系统声音时，扬声器突然停止播放触发的错误
                    LOGE("get_sample GetBuffer error:hr=%d",hr);
                    //break;
                    this->reInit();
                    return -5;
                }


                if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    pData = nullptr;
                }
                if (!pData) {
                    break;
                }

                int pDataLen = numFramesToRead * mWaveFmt->nBlockAlign;
                memcpy(buffer + size, pData, pDataLen);
                size += pDataLen;
                ++i;
                pAudioCaptureClient->ReleaseBuffer(numFramesToRead);
            }
            else {
                break;
            }
        }
        if (0 == size) {
            return -4;
        }

        return 0;

    }
    int WindowsAudioRecorder::get_nb_channels() {
        return mWaveFmt->nChannels;
    }
    int WindowsAudioRecorder::get_nb_bits_sample() {
        return mWaveFmt->wBitsPerSample;
    }
    int WindowsAudioRecorder::get_sample_rate() {
        return mWaveFmt->nSamplesPerSec;
    }
    int WindowsAudioRecorder::get_nb_samples() {
        return nb_samples;
    }
    
    
    IMMDevice* WindowsAudioRecorder::GetDefaultDevice()
    {
        IMMDevice* pDevice = nullptr;

        IMMDeviceEnumerator* pMMDeviceEnumerator = nullptr;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&pMMDeviceEnumerator);
        if (FAILED(hr))
            return nullptr;
        
        if (SOUNDCARD == mCaptureType) {
            hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint((EDataFlow)eRender, eConsole, &pDevice);
        }
        else {
            hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint((EDataFlow)eCapture, eConsole, &pDevice);
        }

        if (pMMDeviceEnumerator)
        {
            pMMDeviceEnumerator->Release();
            pMMDeviceEnumerator = nullptr;
        }

        return pDevice;
    }
    bool WindowsAudioRecorder::AdjustFormat(WAVEFORMATEX* pwfx)
    {
        if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            pwfx->wFormatTag = WAVE_FORMAT_PCM;
        }
        else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
            if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
            {
                pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                pEx->Samples.wValidBitsPerSample = 16;
            }
        }

        pwfx->wBitsPerSample = 16;
        pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
        pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
        
        return true;

    }

}