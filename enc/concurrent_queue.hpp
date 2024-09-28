#ifndef CONCURRENT_QUEUE
#define CONCURRENT_QUEUE

#include <mutex>
#include <vector>

template<typename T>
class ConcurrentQueue
{
public:
	ConcurrentQueue()
		:_curr(0),
		_size(1)
	{
		T t;
		_queue.push_back(t);
	}
	ConcurrentQueue(size_t init_size)
	{
		for (int i = 0; i < init_size + 1; ++i)
		{
			T t;
			_queue.push_back(t);
		}
		_curr = init_size;//指向最尾的元素
		_size = init_size + 1;
	}

	void push(const T& t, bool auto_incr = true)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		//当auto_incr为false时，队列满了不会再增长，也即当队列
		//满的时候，新插入元素会失败
		if (_curr + 1 >= _size)
		{
			if (!auto_incr)
			{
				return;
			}
			else
			{
				incr();
			}
		}
		
		if (_curr + 1 < _size)
		{
			_queue[++_curr] = t;
		}
	}

	T pop(bool& flag)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		//下标为0的内容是无效的
		if (_curr <= 0)
		{
			flag = false;
			return _queue[0];
		}
		flag = true;
		return _queue[_curr--];
	}

private:
	void incr()
	{
		std::vector<T> nv;
		nv.resize(_size * 2);
		for (int i = 0; i < _size; ++i)
		{
			nv[i] = _queue[i];
		}
		_queue = nv;//???
		_size *= 2;
	}

private:
	std::vector<T> _queue;
	std::mutex _mutex;
	int _curr;
	int _size;
};

#endif