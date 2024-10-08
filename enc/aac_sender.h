﻿#ifndef AAC_SENDER_H
#define AAC_SENDER_H

#include <stdint.h>
#include <memory>
#include <thread>
#include <chrono>
#include "../capture/loop_pcm_buffer.h"
#include "threadqueue.hpp"

class AacHelper;
class FileSaver;

class AacSender 
{
public:
	AacSender(int numOfChannels = 2, int sampleRate = 44100, int bitRate = 192000);
	virtual ~AacSender();

	bool isGood();
	void sendFrame(const uint8_t*, int len);
	void startEncThread();
	void stopEncThread();

	virtual void receivePacket(const uint8_t*, int len) = 0;

private:
	void enc_thread();

private:
	AacHelper* _aac_helper;
	bool _is_good;
	ThreadQueue<DcPcmBuffer*> _pcm_queue;
	DcLoopPcmBuffer* _pcm_bufque;
	std::shared_ptr<std::thread> _th;
	bool _is_stop{true};

	int _aac_temp_len{ 2048 };
	uint8_t* _aac_temp_buf;
};

class FileAacSender : public AacSender
{
public:
	FileAacSender(int numOfChannels = 2, int sampleRate = 44100, int bitRate = 192000);

	virtual void receivePacket(const uint8_t*, int len);

private:
	FileSaver* _file;

};

#endif