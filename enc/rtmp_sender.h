#ifndef RTMP_AAC_SENDER_H
#define RTMP_AAC_SENDER_H

#include "aac_sender.h"
#include "concurrent_queue.hpp"

class RtmpPush;

class RtmpAacSender : public AacSender
{
public:
	typedef struct __AacBuf
	{
		__AacBuf()
			:buf(NULL)
		{}
		const uint8_t* buf;
		int len;
	}AacBuf;
	RtmpAacSender(int numOfChannels = 2, int sampleRate = 44100, int bitRate = 192000);
	RtmpAacSender(RtmpPush* pusher, int numOfChannels = 2, int sampleRate = 44100, int bitRate = 192000);

	virtual void receivePacket(const uint8_t*, int len);

	bool connectRtmp(const std::string& url);
	void closeRtmp();

	void startRtmpThread(const std::string& url);
	void stopRtmpThread();

	void setPush(RtmpPush*);

private:
	void send_thread();

	void sendAdtsAac(const uint8_t* aac, int len);

	void init(int size);

private:
	FileSaver* _file;
	ConcurrentQueue<AacBuf> _buf_queue;//空aac包队列
	ThreadQueue<AacBuf> _send_queue;//将要发送的aac adts包
	bool _is_stop{ true };
	std::shared_ptr<std::thread> _th;
	RtmpPush* _pusher{NULL};
	bool _is_rtmp_connect{false};
	std::string _url;

	uint32_t _rtmp_err_count{ 0 };

	//rtmp发送计时等
	int64_t _init_ts{0};
	int64_t _curr_ts{ 0 };
	uint64_t _send_count{ 0 };

};

#endif