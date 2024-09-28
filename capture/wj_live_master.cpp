#include "wj_live_master.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <thread>
#include <iostream>
#include <QList>
#include "loopback_audio.h"
#include "../enc/aac_sender.h"
#include "../enc/rtmp_sender.h"
#include "../enc/rtmp_pusher.h"

#define FRAME_SIZE 4096


DcLiveMaster::DcLiveMaster()
	:_is_stop(true),
	_is_live_on(false),
	_work_thread(std::shared_ptr<std::thread>()),
	_is_pause(false),
	_is_player_pause(true),
	_is_deprecated(false),
	_port_recorder(NULL)
{
	_mix_buf.resize(FRAME_SIZE, '\0');
	_mix_list.push_back(new std::string(FRAME_SIZE, '\0'));
	_mix_list.push_back(new std::string(FRAME_SIZE, '\0'));

	_port_recorder = new PortRecorder(this);
	_pcm_d_mixer = new wymediaServer::CAudioBlockMixer(FRAME_SIZE / 2 / 2, 2);

	_port_main_size = 1024 * 1024;
	_port_main_buff = new char[_port_main_size];

	_port_temp_size = 4096 * 4;
	_port_temp_buff = new char[_port_temp_size];
	_port_main_pos = 0;

	//
	LoopbackAudio::init(44100);
	LoopbackAudio::is_loop = true;
}

void DcLiveMaster::onDestory()
{
	if (_port_recorder)
	{
		delete _port_recorder;
	}
	if (_port_main_buff)
	{
		delete[] _port_main_buff;
		_port_main_buff = NULL;
	}
	if (_port_temp_buff)
	{
		delete[] _port_temp_buff;
		_port_temp_buff = NULL;
	}
	for (int i = 0; i < _mix_list.size(); ++i)
	{
		delete _mix_list[i];
	}
}

void DcLiveMaster::setSender(AacSender* sender)
{
	_aac_sender = sender;
}

void DcLiveMaster::setRtmpSender(RtmpAacSender* sender)
{
	_rtmp_sender = sender;
	//RtmpPush* pusher = new RtmpPush();
	//_rtmp_sender->setPush(pusher);
}

void DcLiveMaster::pauseRecording()
{
	_is_pause = true;
}

void DcLiveMaster::resumeRecording()
{
	_is_pause = false;
}

void DcLiveMaster::setPortRecorder(int index)
{
	_port_device_index = index;
}

void DcLiveMaster::liveStop()
{
	if (_rtmp_sender)
	{
		_rtmp_sender->stopRtmpThread();
	}
	_is_stop = true;
	_port_recorder->stopRecord();
	LoopbackAudio::stop_record();
}

void DcLiveMaster::stopAll()
{
	liveStop();
	//_player.stop();
}

void DcLiveMaster::deprecated()
{
	_is_deprecated = true;
}


void DcLiveMaster::liveOn(const std::string& rtmp_url)
{
	if (_rtmp_sender)
	{
		//std::string("rtmp://pushalphahw.wujiemm.com/live/lxtest-1234")
		_rtmp_sender->startRtmpThread(rtmp_url);
	}
	bool flag = _port_recorder->startRecord(_port_device_index);
	if (flag)
	{
		if (LoopbackAudio::is_good && LoopbackAudio::is_loop && LoopbackAudio::select_index > -1)
		{
			LoopbackAudio::start_record(LoopbackAudio::select_index);
		}
	}
	_is_stop = false;
}

void DcLiveMaster::send_data(const char* buf, int len)
{
	DcPcmBuffer* ptr = NULL;

	//先取本地环回数据
	if (LoopbackAudio::is_good && LoopbackAudio::is_loop && ptr == NULL)
	{
		ptr = LoopbackAudio::loop_bufptr->get_full_buffer();
	}
	//混音并发送
	bool is_mix = false;
	if (ptr)
	{
		//if (true)//没有打开麦克风
		//{
		//	sendPcmData((unsigned char*)ptr->buffer[0], FRAME_SIZE);
		//	LoopbackAudio::loop_bufptr->push_empty_buffer2(ptr);
		//	ptr = NULL;
		//	return;
		//}
		memcpy((void*)_mix_list[0]->c_str(), buf, FRAME_SIZE);
		memcpy((void*)_mix_list[1]->c_str(), ptr->buffer[0], ptr->buff_size);
		//qDebug() << "send pcm, ptr-size:" << ptr->buff_size;

		is_mix = _pcm_d_mixer->Process(_mix_list, _mix_buf);
		if (is_mix) 
		{
			sendPcmData((unsigned char*)_mix_buf.c_str(), FRAME_SIZE);
		}
		else
		{
			sendPcmData((unsigned char*)buf, FRAME_SIZE);
		}
		LoopbackAudio::loop_bufptr->push_empty_buffer2(ptr);
		ptr = NULL;
	}
	else
	{
		qDebug() << "BGM is null...";
		sendPcmData((unsigned char*)buf, len);
	}
}

void DcLiveMaster::sendPcmData(uint8_t* buf, int len)
{
	/*if (_aac_sender)
	{
		_aac_sender->sendFrame(buf, len);
	}*/
	int64_t now = timeGetTime();
	if (now - _last_tick > 65)
	{
		std::cout << "time diff: " << now - _last_tick << std::endl;
	}
	_last_tick = now;
	if (_rtmp_sender)
	{
		//int64_t t1 = timeGetTime();
		_rtmp_sender->sendFrame(buf, len);
		/*int64_t t2 = timeGetTime();
		if (t2 - t1 > 10)
		{
			std::cout << "awake time: " << t2 - t1 << std::endl;
		}*/
	}
}

//port录音回调
void DcLiveMaster::stream_cb(const void* input, unsigned long frameCount, int sampleSize)
{
	//qDebug() << "stream cb, framecount: " << frameCount;
	return stream_cb2(input, frameCount, sampleSize);
}

void DcLiveMaster::stream_cb2(const void* input, unsigned long frameCount, int sampleSize)
{
	//qDebug() << "stream cb2, framecount: " << frameCount;
	
	if (_is_stop)
	{
		return;
	}

	int new_port_pos = 0;
	int samples_size = frameCount * 2 * sampleSize / 8;
	//qDebug() << "stream_cb2 samples_size: " << samples_size << "\tport_pos: " << _port_main_pos;
	if (samples_size > 0)
	{
		//是否要重置buff
		if (_port_main_pos + samples_size > _port_main_size)
		{
			int content_len = _port_main_size - _port_main_pos;
			int min = content_len > _port_temp_size ? _port_temp_size : content_len;
			memcpy(_port_temp_buff, _port_main_buff + _port_main_pos, min);
			memcpy(_port_main_buff, _port_temp_buff, min);
			_port_main_pos = min;
			_port_main_read_pos = 0;
		}
		if (_port_main_pos + samples_size <= _port_main_size)
		{
			memcpy(_port_main_buff + _port_main_pos, input, samples_size);
			_port_main_pos += samples_size;
		}
		if (_port_main_pos - _port_main_read_pos >= 4096)//4096
		{
			send_data(_port_main_buff + _port_main_read_pos, 4096);
			_port_main_read_pos += 4096;
		}
	}
}

DcLiveMaster::~DcLiveMaster()
{
}
