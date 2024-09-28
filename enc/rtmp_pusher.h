#ifndef RTMP_PUSHER_H
#define RTMP_PUSHER_H
#include "srs_librtmp.h"
#include <string>

class RtmpPush 
{
public:
	bool connect(const std::string&);

	int pushAac(const uint8_t* aac, int len, int pts);

	void destory();

private:
	srs_rtmp_t _context{NULL};

};

#endif