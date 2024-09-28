#include "rtmp_pusher.h"
#include <iostream>

bool RtmpPush::connect(const std::string& url)
{
	srs_rtmp_t rtmp = srs_rtmp_create(url.c_str());
	if (rtmp == NULL)
	{
		srs_human_trace("create rtmp failed.");
		return false;
	}
	if (srs_rtmp_set_timeout(rtmp, 12000, 1000) != 0)
	{
		srs_human_trace("set timeout failed.");
		return false;
	}
	if (srs_rtmp_handshake(rtmp) != 0) 
	{
		srs_human_trace("simple handshake failed.");
		return false;
	}
	srs_human_trace("simple handshake success");

	if (srs_rtmp_connect_app(rtmp) != 0) 
	{
		srs_human_trace("connect vhost/app failed.");
		return false;
	}
	if (srs_rtmp_publish_stream(rtmp) != 0)
	{
		srs_human_trace("publish stream failed.");
		return false;
	}
	_context = rtmp;

	return true;
}

/*************************************************************
**************************************************************
* audio raw codec
**************************************************************
*************************************************************/
/**
* write an audio raw frame to srs.
* not similar to h.264 video, the audio never aggregated, always
* encoded one frame by one, so this api is used to write a frame.
*
* @param sound_format Format of SoundData. The following values are defined:
*               0 = Linear PCM, platform endian
*               1 = ADPCM
*               2 = MP3
*               3 = Linear PCM, little endian
*               4 = Nellymoser 16 kHz mono
*               5 = Nellymoser 8 kHz mono
*               6 = Nellymoser
*               7 = G.711 A-law logarithmic PCM
*               8 = G.711 mu-law logarithmic PCM
*               9 = reserved
*               10 = AAC
*               11 = Speex
*               14 = MP3 8 kHz
*               15 = Device-specific sound
*               Formats 7, 8, 14, and 15 are reserved.
*               AAC is supported in Flash Player 9,0,115,0 and higher.
*               Speex is supported in Flash Player 10 and higher.
* @param sound_rate Sampling rate. The following values are defined:
*               0 = 5.5 kHz
*               1 = 11 kHz
*               2 = 22 kHz
*               3 = 44 kHz
* @param sound_size Size of each audio sample. This parameter only pertains to
*               uncompressed formats. Compressed formats always decode
*               to 16 bits internally.
*               0 = 8-bit samples
*               1 = 16-bit samples
* @param sound_type Mono or stereo sound
*               0 = Mono sound
*               1 = Stereo sound
* @param timestamp The timestamp of audio.
*
* @example /trunk/research/librtmp/srs_aac_raw_publish.c
* @example /trunk/research/librtmp/srs_audio_raw_publish.c
*
* @remark for aac, the frame must be in ADTS format.
*       @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 75, 1.A.2.2 ADTS
* @remark for aac, only support profile 1-4, AAC main/LC/SSR/LTP,
*       @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23, 1.5.1.1 Audio object type
*
* @see https://github.com/ossrs/srs/issues/212
* @see E.4.2.1 AUDIODATA of video_file_format_spec_v10_1.pdf
*
* @return 0, success; otherswise, failed.
*/
int RtmpPush::pushAac(const uint8_t* aac, int len, int pts)
{
	int ret = srs_audio_write_raw_frame(_context, 10, 3, 1, 1, (char*)(aac), len, pts);
	if (ret != 0)
	{
		std::cout << "push data error." << std::endl;
	}
	return ret;
}

void RtmpPush::destory()
{
	srs_rtmp_destroy(_context);
}