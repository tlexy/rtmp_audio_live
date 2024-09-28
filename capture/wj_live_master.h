#pragma once

#ifndef DCLIVEMASTER_HH_
#define DCLIVEMASTER_HH_

#include <thread>
#include "audio_internal_mixer.h"

#include "iport_callback.h"
#include "port_recorder.h"

#pragma execution_character_set("utf-8")


class AacSender;
class RtmpAacSender;

class DcLiveMaster : public IPortCallBack
{
public:
	explicit DcLiveMaster();
	void setPortRecorder(int index);
	void setSender(AacSender*);
	void setRtmpSender(RtmpAacSender* sender);

	void send_data(const char* buf, int len);

	void sendPcmData(uint8_t*, int len);

	void onDestory();

	//暂停录音
	void pauseRecording();
	//重新开始录音
	void resumeRecording();

	//开始直播
	void liveOn(const std::string& rtmp_url);
	//停止直播
	void liveStop();
	void stopAll();
	void deprecated();

	//port recorder...
	virtual void stream_cb(const void* input, unsigned long frameCount, int sampleSize);
	void stream_cb2(const void* input, unsigned long frameCount, int sampleSize);

private:
	bool _is_stop;
	bool _is_live_on;
	bool _is_pause;
	bool _is_player_pause;
	bool _is_deprecated;
	std::shared_ptr<std::thread> _work_thread;
	std::string _record_path;
	std::wstring _record_wpath;

	AacSender* _aac_sender{NULL};
	RtmpAacSender* _rtmp_sender{ NULL };

	int64_t _last_tick{0};

	wymediaServer::IAudioBlockMixer* _pcm_d_mixer;

	int _port_device_index;
	PortRecorder* _port_recorder{NULL};
	char* _port_main_buff;//1M
	char* _port_temp_buff;//16K
	int _port_temp_size;
	int _port_main_size;
	int _port_main_pos;
	int _port_main_read_pos{0};
	bool _is_jetter_enough{ false };
	int _port_pos{0};
	int _port_index{0};
	wymediaServer::AudioBlockList _mix_list;
	std::string _mix_buf;

public:
	~DcLiveMaster();
};

#endif

