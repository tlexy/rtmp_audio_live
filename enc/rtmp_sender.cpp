#include <windows.h>
#include "rtmp_sender.h"
#include "aac_helper.h"
#include "file_saver.h"
#include <iostream>
#include "rtmp_pusher.h"


RtmpAacSender::RtmpAacSender(int numOfChannels, int sampleRate, int bitRate)
	:AacSender(numOfChannels, sampleRate, bitRate),
	_th(std::shared_ptr<std::thread>()),
	_send_queue(43)
{
	init(50);
}

RtmpAacSender::RtmpAacSender(RtmpPush* pusher, int numOfChannels, int sampleRate, int bitRate)
	:AacSender(numOfChannels, sampleRate, bitRate),
	_th(std::shared_ptr<std::thread>()),
	_send_queue(43),
	_pusher(pusher)
{
	init(50);
}

void RtmpAacSender::init(int size)
{
	_file = new FileSaver(1024 * 1024*10, "demo");
	for (int i = 0; i < size; ++i)
	{
		AacBuf buf;
		buf.buf = (uint8_t*)malloc(2048);
		buf.len = 2048;
		_buf_queue.push(buf);
	}
}

void RtmpAacSender::startRtmpThread(const std::string& url)
{
	if (_th)
	{
		return;
	}
	_url = url;
	_is_rtmp_connect = connectRtmp(url);
	_th = std::make_shared<std::thread>(&RtmpAacSender::send_thread, this);
}

void RtmpAacSender::stopRtmpThread()
{
	closeRtmp();
	if (_th)
	{
		_is_stop = true;
		AacBuf aac;
		aac.buf = NULL;
		aac.len = 0;
		_send_queue.push_back(aac);

		_th->join();
		_th = std::shared_ptr<std::thread>();
	}
}

void RtmpAacSender::send_thread()
{
	bool flag = false;
	_is_stop = false;
	while (!_is_stop)
	{
		if (!_is_rtmp_connect)
		{
			_is_rtmp_connect = connectRtmp(_url);
		}
		AacBuf aac_buf = _send_queue.pop(flag, std::chrono::milliseconds(100));
		if (flag && aac_buf.len > 0 && aac_buf.buf != NULL)
		{
			//发送数据
			sendAdtsAac(aac_buf.buf, aac_buf.len);
			aac_buf.len = 2048;
			_buf_queue.push(aac_buf, false);
		}
		else
		{
			std::cout << "something error 1" << std::endl;
			if (!flag)
			{
				std::cout << "get empty buf." << std::endl;
			}
			else
			{
				std::cout << "aac_buf.len: " << aac_buf.len << std::endl;
			}
		}
	}
}

void RtmpAacSender::receivePacket(const uint8_t* buf, int len)
{
	if (_is_stop)
	{
		return;
	}
	//std::cout << "receivePacket, len: " << len << std::endl;
	//首先要将内存复制一份
	bool flag = false;
	AacBuf aac = _buf_queue.pop(flag);
	if (!flag || aac.buf == NULL)
	{
		std::cout << "get aac buf NULL." << std::endl;
		return;
	}
	if (len > aac.len)
	{
		std::cout << "aac len to long. real len" << len << "\tbuf len: " << aac.len << std::endl;
		return;
	}
	memcpy((void*)aac.buf, buf, len);
	aac.len = len;

	//当队列满的时候，Push会失败
	flag = _send_queue.push_back_e(aac);
	if (!flag)
	{
		std::cout << "send queue is full, maybe network is too slow." << std::endl;
		aac.len = 2048;
		_buf_queue.push(aac);
	}
}

void RtmpAacSender::sendAdtsAac(const uint8_t* aac, int len)
{
	_file->write((const char*)aac, len);
	if (!_is_rtmp_connect)
	{
		return;
	}
	int pts = 0;
	if (_send_count == 0 || _init_ts == 0)
	{
		_init_ts = timeGetTime();
		pts = 0;
	}
	else
	{
		pts = timeGetTime() - _init_ts;
	}
	++_send_count;
	if (_pusher)
	{
		int ret = _pusher->pushAac(aac, len, pts);
		if (ret != 0)
		{
			++_rtmp_err_count;
			if (_rtmp_err_count >= 5)
			{
				_is_rtmp_connect = false;
				_rtmp_err_count = 0;
				closeRtmp();
			}
		}
	}
}

void RtmpAacSender::setPush(RtmpPush* pusher)
{
	_pusher = pusher;
}

bool RtmpAacSender::connectRtmp(const std::string& url)
{
	if (_pusher)
	{
		return _pusher->connect(url);
	}
	return false;
}

void RtmpAacSender::closeRtmp()
{
	_file->save();
	_is_rtmp_connect = false;
	if (_pusher)
	{
		return _pusher->destory();
	}
}