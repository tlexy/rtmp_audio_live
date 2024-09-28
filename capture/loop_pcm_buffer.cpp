#include "loop_pcm_buffer.h"
#include <QDebug>

DcPcmBuffer::DcPcmBuffer(int size)
	:buff_size(size)
{
	buffer[0] = (uint8_t*)malloc(size);
	memset(buffer[0], 0x0, size);
}

DcLoopPcmBuffer::DcLoopPcmBuffer(int queue_size, int out_size)
	:_full_write(0),
	_full_read(0),
	_full_out_size(out_size),
	_is_enough(false),
	_inner_pos(0),
	_single_size(4096),
	_inner_unspecified_pos(0)
{
	for (int i = 0; i < queue_size; ++i)
	{
		_empty_buff.push_back(new DcPcmBuffer(4096));
		_full_buff.push_back(new DcPcmBuffer(4096));
		_empty_state.push_back(true);
	}
	int a = 1;
	_empty_top = queue_size - 1;

	_inner_mid = new mid_buf(1024 * 1024);
}

void DcLoopPcmBuffer::reset_wait()
{}

void DcLoopPcmBuffer::reset()
{}

void DcLoopPcmBuffer::push_unspecified(const char* data, int len)
{
	if (len < 1)
	{
		return;
	}
	int total = len;
	int buf_rest_len = 0;
	int in_data_pos = 0;
	int copy_len = 0;
	while (total > 0)
	{
		buf_rest_len = UNSPECIFIED_LEN - _inner_unspecified_pos;
		copy_len = total > buf_rest_len ? buf_rest_len : total;
		memcpy(_inner_unspecified_buf + _inner_unspecified_pos, data + in_data_pos, copy_len);
		total -= copy_len;
		_inner_unspecified_pos += copy_len;
		if (_inner_unspecified_pos >= 4096)
		{
			write_to_buffer();
		}
	}
}

void DcLoopPcmBuffer::push_unspecified2(const char* data, int len)
{
	if (len < 1)
	{
		return;
	}
	_inner_mid->push((const uint8_t*)data, len);
	if (_inner_mid->usable_count() >= 4096)
	{
		write_to_buffer2();
	}
}

void DcLoopPcmBuffer::write_to_buffer2()
{
	while (_inner_mid->usable_count() >= 4096)
	{
		DcPcmBuffer* ptr = get_empty_buffer2();
		if (ptr != NULL)
		{
			memcpy(ptr->buffer[0], _inner_mid->data(), 4096);
			push_full_buffer(ptr);
			_inner_mid->set_read(4096);
		}
		else
		{
			break;
		}
	}
}

void DcLoopPcmBuffer::write_to_buffer()
{
	int buf_len = _inner_unspecified_pos;
	int pos = 0;
	//每4096一个buff_ptr
	while (buf_len >= 4096)
	{
		DcPcmBuffer* ptr = get_empty_buffer2();
		if (ptr != NULL)
		{
			memcpy(ptr->buffer[0], _inner_unspecified_buf + pos, 4096);
			push_full_buffer(ptr);
			pos += 4096;
			buf_len -= 4096;
		}
		else
		{
			return;
		}
	}
	if (buf_len > 0)
	{
		memcpy(_inner_temp, _inner_unspecified_buf + pos, buf_len);
		memcpy(_inner_unspecified_buf, _inner_temp, buf_len);
		_inner_unspecified_pos = buf_len;
	}
}

void DcLoopPcmBuffer::push(const char* data, int len)
{
	if (len > 4096 || len < 1)
	{
		return;
	}
	/*DcPcmBuffer* ptr = get_empty_buffer();
	if (ptr == NULL)
	{
		return;
	}
	memcpy(ptr->buffer[0], data, len);
	push_full_buffer(ptr);*/
	if (_inner_pos + len > sizeof(_inner_buf))
	{
		//数据太多了
		//qDebug() << "data to big";
	}
	else
	{
		memcpy(_inner_buf + _inner_pos, data, len);
		//qDebug() << "aaa, pos: " << _inner_pos << "\tlen: " << len;
		_inner_pos += len;
	}
	//qDebug() << "bbb, pos: " << _inner_pos << "\tlen: " << len;
	if (_inner_pos >= 4096)
	{
		DcPcmBuffer* ptr = get_empty_buffer();
		if (ptr != NULL)
		{
			memcpy(ptr->buffer[0], _inner_buf, 4096);
			push_full_buffer(ptr);
			
			//将剩余的拷贝到临时缓存，再拷贝回来
			int rest = _inner_pos - 4096;
			//qDebug() << "ccc, pos: " << _inner_pos << "\rest: " << rest;
			if (rest > 0)
			{
				memcpy(_inner_temp, _inner_buf + 4096, rest);
				memcpy(_inner_buf, _inner_temp, rest);
				_inner_pos = rest;
			}
			else
			{
				_inner_pos = 0;
			}
		}
		/*else
		{
			_inner_pos -= len;
		}*/
	}
	
}

void DcLoopPcmBuffer::push_full_buffer(DcPcmBuffer* ptr)
{
	if (!ptr)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(_full_mutex);
	//重置，重新等待足够数据
	if (_full_write == _full_read)
	{
		_is_enough = false;
	}
	//如果满了
	if (_full_write >= _full_buff.size())
	{
		int idx = 0;
		int min = _full_write > _full_buff.size() ? _full_buff.size() : _full_write;
		for (int i = _full_read; i < min; ++i)
		{
			_full_buff[idx] = _full_buff[i];
			++idx;
		}
		_full_write = _full_write - _full_read;
		_full_read = 0;
	}

	if (_full_write < _full_buff.size())
	{
		_full_buff[_full_write] = ptr;
		++_full_write;
	}
	else
	{
		//qDebug() << "lose ptr...";
	}
	if (_full_write - _full_read >= _full_out_size)
	{
		_is_enough = true;
	}
}

DcPcmBuffer* DcLoopPcmBuffer::get_full_buffer()
{
	if (!_is_enough)
	{
		return NULL;
	}
	std::lock_guard<std::mutex> lock(_full_mutex);
	DcPcmBuffer* ptr = NULL;
	if (_full_read < _full_write && _full_read < _full_buff.size())
	{
		ptr = _full_buff[_full_read];
		++_full_read;
		//qDebug() << "read idx: " <<  _full_read << "\tlen:" << ptr->buff_size;
	}
	return ptr;
}

DcPcmBuffer* DcLoopPcmBuffer::get_empty_buffer()
{
	std::lock_guard<std::mutex> lock(_empty_mutex);
	for (int i = 0; i < _empty_state.size(); ++i)
	{
		if (_empty_state[i])
		{
			_empty_state[i] = false;//标记内容被取出
			return _empty_buff[i];
		}
	}
	return NULL;
}

void DcLoopPcmBuffer::push_empty_buffer(DcPcmBuffer* ptr)
{
	std::lock_guard<std::mutex> lock(_empty_mutex);
	for (int i = 0; i < _empty_state.size(); ++i)
	{
		if (_empty_state[i] == false)
		{
			_empty_state[i] = true;//标记内容被归还
			_empty_buff[i] = ptr;
			break;
		}
	}
}

DcPcmBuffer* DcLoopPcmBuffer::get_empty_buffer2()
{
	std::lock_guard<std::mutex> lock(_empty_mutex);
	if (_empty_top > -1 && _empty_top < _empty_buff.size())
	{
		return _empty_buff[_empty_top--];
	}
	return NULL;
}

void DcLoopPcmBuffer::push_empty_buffer2(DcPcmBuffer* ptr)
{
	std::lock_guard<std::mutex> lock(_empty_mutex);
	if (_empty_top >= -1 && _empty_top < _empty_buff.size() - 1)
	{
		_empty_buff[++_empty_top] = ptr;
	}
	else
	{
		_empty_top = 0;
		_empty_buff[_empty_top] = ptr;
	}
}