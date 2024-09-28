#ifndef LOOPBACK_AUDIO_H
#define LOOPBACK_AUDIO_H

#include <iostream>
#include <QString>
#include <vector>
#include <map>
#include <thread>
#include <memory>
#include <chrono>
#include <portaudio.h>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include "loop_pcm_buffer.h"
#include <windows.h>

#pragma execution_character_set("utf-8")

class LoopbackAudio
{
public:
	typedef struct __DeviceInfo
	{
		int pa_index;
		const PaDeviceInfo* device;
	}DeviceInfo;
	LoopbackAudio();
	static void init(int dst_rate);
	static void destory();

	static void start_record(int index);
	static void stop_record();

	//录音线程+重采样？
	static void thread_record(int index);

	static struct SwrContext* init_swr(int src_rate, int src_channels, int src_fmt, int dst_rate, int dst_channels, int dst_fmt);
	static int do_swr(struct SwrContext* swr, void* in_data, int nb_in_samples, int src_rate, void* out_data, int out_len, int dst_rate);

	static void re_init_buf(int size);

public:
	static bool is_good;
	static std::map<int, const PaDeviceInfo*> device_list;
	static std::vector<DeviceInfo> device_vec;
	static std::shared_ptr<std::thread> _record_thread;
	static int _src_rate;
	static int _dst_rate;
	static bool _is_record_stop;
	static int _error_count;
	static struct SwrContext* _swr_ctx;
	static std::shared_ptr<DcLoopPcmBuffer> loop_bufptr;
	static int select_index;//当用户选择一个背景音设备时，相应的设备号会被记录下来
	static bool is_loop;//当用户勾选“背景音”时，这个字段为true
	static bool _is_init;
	static PaStream* _stream;

	//不固定的帧大小
	static char* record_frame_buff;
	static int record_frame_len;
	static char* swr_temp_buff;
	static int swr_temp_len;

};

#endif