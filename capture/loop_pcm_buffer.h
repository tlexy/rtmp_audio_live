#ifndef DC_LOOP_PCM_BUFFER_H
#define DC_LOOP_PCM_BUFFER_H

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <mutex>
#include "mid_buf.h"

#define UNSPECIFIED_LEN 65536 //(4096*16)

class DcPcmBuffer
{
public:
	DcPcmBuffer(int buff_size);

public:
	uint8_t* buffer[1];
	int buff_size;
};

class DcLoopPcmBuffer
{
public:
	DcLoopPcmBuffer(int queue_size, int out_size);

	DcPcmBuffer* get_empty_buffer();
	void push_empty_buffer(DcPcmBuffer*);

	DcPcmBuffer* get_empty_buffer2();
	void push_empty_buffer2(DcPcmBuffer*);

	DcPcmBuffer* get_full_buffer();
	void push_full_buffer(DcPcmBuffer*);

	//推入数据
	void push(const char* data, int len);

	void push_unspecified(const char* data, int len);

	void push_unspecified2(const char* data, int len);

private:
	//重新开始等待足够的数据
	void reset_wait();

	void write_to_buffer();
	void write_to_buffer2();

	//清空当次缓存
	void reset();

private:
	std::vector<DcPcmBuffer*> _empty_buff;
	std::vector<bool> _empty_state;
	std::vector<DcPcmBuffer*> _full_buff;//只在当full_buff达到一定大小时，才进行输出？
	int _full_write;
	int _full_read;
	int _full_out_size;//数据量达到这个大小时，才开始输出数据
	bool _is_enough;
	std::mutex _full_mutex;
	std::mutex _empty_mutex;
	int _empty_top{-1};
	uint8_t _inner_buf[4096 * 2*2];
	uint8_t _inner_unspecified_buf[UNSPECIFIED_LEN];
	uint8_t _inner_temp[4096];
	int _inner_pos;
	int _inner_unspecified_pos{0};//pos总是指向可用数据长度
	int _single_size{4096};

	mid_buf* _inner_mid;
};

#endif