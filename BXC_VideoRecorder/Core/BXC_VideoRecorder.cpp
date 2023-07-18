#include "../include/BXC_VideoRecorder.h"
#include "Intercept.h"
#include "WindowsDXGI.h"
#include "WindowsGDI.h"
#include "FFmpegRecorder.h"
#include "Common.h"
#include <map>
#define MESSAGE "build BXC_VideoRecorder"
#pragma message(MESSAGE)

namespace BXC_MediaLibrary {

	struct ObjManage {
	public:
		ObjManage() {

		}
		~ObjManage() {

		}
	public:
		std::map<int, Intercept*> objMap;
	public:
		int getSize() {
			return objMap.size();
		}
		bool remove(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					Intercept* obj = f->second;
					delete obj;
					obj = nullptr;

					objMap.erase(f);
					return true;
				}
			}
			return false;
		}
		Intercept* get(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					Intercept* obj = f->second;
					return obj;
				}
			}
			return nullptr;
		}
		int add(Intercept* obj) {
			int count = objMap.size() + 1001;
			objMap[count] = obj;
			return count;
		}
	};
	ObjManage objManage;

	int BXC_VideoRecorder_Open(BXC_VideoRecorder* recorder) {
		
		Intercept* obj = nullptr;
		if (0==strcmp("DXGI", recorder->capture)) {
			obj = new WindowsDXGI(recorder->capture, recorder->width, recorder->height);
		}else if (0 == strcmp("GDI", recorder->capture)) {
			obj = new WindowsGDI(recorder->capture, recorder->width, recorder->height);
		}else {
			obj = new FFmpegRecorder(recorder->capture, recorder->width, recorder->height);
		}
		recorder->id = objManage.add(obj);
		LOGI("(capture=%s,width=%d,height=%d,idx=%d) id=%d,objSize=%d", 
			recorder->capture, recorder->width, recorder->height, recorder->idx,
			recorder->id, objManage.getSize());

		if (!obj->init(recorder->idx))
		{
			BXC_VideoRecorder_Close(recorder);
			return -1;
		}
		else {
			recorder->pixelFormat = (BXC_PixelFormat)obj->getPixelFormat();
			recorder->factWidth = obj->getFactWidth();
			recorder->factHeight = obj->getFactHeight();
		}
		return 0;

	}
	int BXC_VideoRecorder_Close(BXC_VideoRecorder* recorder) {
		LOGI("id=%d,objSize=%d", recorder->id, objManage.getSize());

		if (objManage.remove(recorder->id)) {
			return 0;
		}
		return -1;
	}

	int BXC_get_frame(BXC_VideoRecorder* recorder, unsigned char*& buffer, int& size, int64_t& timestamp)
	{
		Intercept* obj = objManage.get(recorder->id);
		if (obj) {
			if (obj->getFrame(buffer, size, timestamp)) {
				return 0;
			}
			else {
				return -2;
			}
		}
		return -1;
	}
}