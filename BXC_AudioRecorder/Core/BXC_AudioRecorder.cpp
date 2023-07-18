#include "../include/BXC_AudioRecorder.h"
#include "WindowsAudioRecorder.h"
#include "Common.h"
#include <map>
#define MESSAGE "build BXC_AudioRecorder"
#pragma message(MESSAGE)

namespace BXC_MediaLibrary {

	struct ObjManage {
	public:
		ObjManage() {

		}
		~ObjManage() {
	
		}
	public:
		std::map<int, WindowsAudioRecorder*> objMap;
	public:
		int getSize() {
			return objMap.size();
		}
		bool remove(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					WindowsAudioRecorder* obj = f->second;
					delete obj;
					obj = nullptr;

					objMap.erase(f);
					return true;
				}
			}
			return false;
		}
		WindowsAudioRecorder* get(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					WindowsAudioRecorder* obj = f->second;
					return obj;
				}
			}
			return nullptr;
		}
		int add(WindowsAudioRecorder* obj) {
			int count = objMap.size() + 1001;
			objMap[count] = obj;
			return count;
		}
	};
	ObjManage objManage;

	int __DECLSPEC_INC BXC_AudioRecorder_Open(BXC_AudioRecorder* recorder){
		CoInitialize(nullptr);

		WindowsAudioRecorder* obj;
		if (0 == strcmp("SOUNDCARD", recorder->capture)) {
			obj = new WindowsAudioRecorder(WindowsAudioRecorder::SOUNDCARD);
		}
		else if (0 == strcmp("MICROPHONE", recorder->capture)) {
			obj = new WindowsAudioRecorder(WindowsAudioRecorder::MICROPHONE);
		}
		else {
			LOGE("unsupported capture=%s", recorder->capture);
			return -1;
		}
		recorder->id = objManage.add(obj);
		recorder->nb_channels = obj->get_nb_channels();
		recorder->nb_bits_sample = obj->get_nb_bits_sample();
		recorder->sample_rate = obj->get_sample_rate();
		recorder->nb_samples = obj->get_nb_samples();

		LOGI("(capture=%s) id=%d,objSize=%d", recorder->capture, recorder->id, objManage.getSize());

		return 0;

	}
	int BXC_AudioRecorder_Close(BXC_AudioRecorder* recorder) {
		LOGI("id=%d,objSize=%d", recorder->id, objManage.getSize());
		CoUninitialize();
		if (objManage.remove(recorder->id)) {
			return 0;
		}
		return -1;
	}
	int BXC_get_sample(BXC_AudioRecorder* recorder,unsigned char*& buffer, int& size, int64_t& timestamp)
	{
		WindowsAudioRecorder* obj = objManage.get(recorder->id);
		if (obj) {
			return obj->get_sample(buffer, size, timestamp);
		}
		return -1;

	}
	int64_t BXC_audio_timestamp() {
		return getCurTimestamp();
	}
}