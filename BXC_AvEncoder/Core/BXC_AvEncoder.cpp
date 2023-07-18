#include "../include/BXC_AvEncoder.h"
#include "FFmpegEncoder.h"
#include "Common.h"
#include <map>
#define MESSAGE "build BXC_AvEncoder"
#pragma message(MESSAGE)

namespace BXC_MediaLibrary {
	struct ObjManage {
	public:
		ObjManage() {

		}
		~ObjManage() {

		}
	public:
		std::map<int, FFmpegEncoder*> objMap;
	public:
		int getSize() {
			return objMap.size();
		}
		bool remove(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					FFmpegEncoder* obj = f->second;
					delete obj;
					obj = nullptr;

					objMap.erase(f);
					return true;
				}
			}
			return false;
		}
		FFmpegEncoder* get(int id) {
			for (auto f = objMap.begin(); f != objMap.end(); ++f) {
				if (id == f->first) {
					FFmpegEncoder* obj = f->second;
					return obj;
				}
			}
			return nullptr;
		}
		int add(FFmpegEncoder* obj) {
			int count = objMap.size() + 1001;
			objMap[count] = obj;
			return count;
		}
	};
	ObjManage objManage;

	int BXC_AvEncoder_Open(BXC_AvEncoder* encoder, const char* url) {

		FFmpegEncoder* obj = new FFmpegEncoder;
		encoder->id = objManage.add(obj);
		LOGI("id=%d,objSize=%d", encoder->id, objManage.getSize());
		
		if (obj->start(encoder,url)) {
			return 0;
		}else {
			BXC_AvEncoder_Close(encoder);
			return -1;
		}
	}
	int BXC_AvEncoder_Close(BXC_AvEncoder* encoder) {
		LOGI("id=%d,objSize=%d", encoder->id, objManage.getSize());
		FFmpegEncoder* obj = objManage.get(encoder->id);
		if (obj) {
			obj->stop();//阻塞停止编码器
			return objManage.remove(encoder->id);
		}
		return -1;
	}

	int64_t BXC_gettime() {
		return FFmpegEncoder::gettime();
	}
	int BXC_send_frame(BXC_AvEncoder* encoder, BXC_AvFrame* frame) {
		FFmpegEncoder* obj = objManage.get(encoder->id);
		if (obj) {
			if (AVVIDEO == frame->type && obj->getHasVideo()) {
				BXC_AvFrame* newFrame = new BXC_AvFrame(frame->type, frame->size, frame->timestamp, frame->count);
				memcpy(newFrame->data, frame->data, frame->size);
				obj->sendFrame(newFrame);
				return 0;
			}
			else if (AVAUDIO == frame->type && obj->getHasAudio()) {
				BXC_AvFrame* newFrame = new BXC_AvFrame(frame->type, frame->size, frame->timestamp, frame->count);
				memcpy(newFrame->data, frame->data, frame->size);
				obj->sendFrame(newFrame);
				return 0;
			}
			else {
				return -2;
			}
		}
		else {
			return -1;
		}

	}

	int BXC_receive_packet(BXC_AvEncoder* encoder, BXC_AvPacket* packet) {

		FFmpegEncoder* obj = objManage.get(encoder->id);
		if (obj) {
			BXC_AvPacket* oldPacket = nullptr;
			if (AVVIDEO == packet->type && obj->getHasVideo()) {
				oldPacket = obj->getVideoPacket();
			}
			else if (AVAUDIO == packet->type && obj->getHasAudio()) {
				oldPacket = obj->getAudioPacket();
			}
			else {
				return -2;
			}
			if (oldPacket) {
				memcpy(packet->data, oldPacket->data, oldPacket->size);
				packet->size = oldPacket->size;
				packet->stream_index = oldPacket->stream_index;
				packet->pts = oldPacket->pts;
				packet->dts = oldPacket->dts;
				packet->duration = oldPacket->duration;
				packet->pos = oldPacket->pos;
				packet->timestamp = oldPacket->timestamp;
				packet->count = oldPacket->count;

				delete oldPacket;
				oldPacket = nullptr;

				return 0;
			}
			else {
				return -3;
			}

		}
		else {
			return -1;
		}
	}

}