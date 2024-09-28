#include "loopback_audio.h"
#include <QDebug>
#include <time.h>

#pragma execution_character_set("utf-8")

#define SAMPLES_PER_FRAME 1024

//#define LOOP_BACK_PRINT_TEST

//#define PCM_FLOAT32

bool LoopbackAudio::is_good = true;
std::map<int, const PaDeviceInfo*> LoopbackAudio::device_list;
std::vector<LoopbackAudio::DeviceInfo> LoopbackAudio::device_vec;
std::shared_ptr<std::thread> LoopbackAudio::_record_thread = std::shared_ptr<std::thread>();
int LoopbackAudio::_src_rate;
int LoopbackAudio::_dst_rate;
bool LoopbackAudio::_is_record_stop = true;
int LoopbackAudio::_error_count = 0;
struct SwrContext* LoopbackAudio::_swr_ctx = NULL;
std::shared_ptr<DcLoopPcmBuffer> LoopbackAudio::loop_bufptr;
bool LoopbackAudio::is_loop = false;
bool LoopbackAudio::_is_init = false;
int LoopbackAudio::select_index = -1;
PaStream* LoopbackAudio::_stream = NULL;

char* LoopbackAudio::record_frame_buff = NULL;
int LoopbackAudio::record_frame_len = 0;
char* LoopbackAudio::swr_temp_buff = NULL;
int LoopbackAudio::swr_temp_len = 0;

#define PCM_CHANNEL_BIT 4


static int loop_sample_size = 4;

LoopbackAudio::LoopbackAudio()
{}

void printf_device_info(int index_num, const PaDeviceInfo* dinfo)
{
	if (!dinfo)
	{
		return;
	}
	/*log_w("PaAudio info: maxInputChannels: device_index: %d, name: %s, hostAPi: %d, maxInputChannels: %d, maxOutputChannels: %d, defaultLowInputLatency: %f, defaultLowOutputLatency: %f, defaultHighInputLatency: %f, defaultHighOutputLatency: %f, defaultSampleRate: %f",
		index_num, dinfo->name, dinfo->hostApi, dinfo->maxInputChannels, dinfo->maxOutputChannels, dinfo->defaultLowInputLatency, dinfo->defaultLowOutputLatency, dinfo->defaultHighInputLatency, dinfo->defaultHighOutputLatency, dinfo->defaultSampleRate);
		*/
}

void LoopbackAudio::init(int dst_rate)
{
	if (_is_init)
	{
		return;
	}

	if (record_frame_len == 0 && record_frame_buff == NULL)
	{
		record_frame_len = 48000 * 4 * 2;//48K,1s的数据，按最大32位算
		swr_temp_len = record_frame_len;
		record_frame_buff = (char*)malloc(record_frame_len);
		swr_temp_buff = (char*)malloc(swr_temp_len);
	}

	_dst_rate = dst_rate;
	loop_bufptr = std::make_shared<DcLoopPcmBuffer>(30, 3);

	PaError err = Pa_Initialize();
	if (err != paNoError)
	{
		is_good = false;
	}
	PaHostApiIndex hostCnt = Pa_GetHostApiCount();
	for (int i = 0; i < hostCnt; ++i)
	{
		const PaHostApiInfo* hinfo = Pa_GetHostApiInfo(i);
		if (hinfo->type == paWASAPI/* || hinfo->type == paDirectSound*/)
		{
			for (PaDeviceIndex hostDevice = 0; hostDevice < hinfo->deviceCount; ++hostDevice)
			{
				PaDeviceIndex deviceNum = Pa_HostApiDeviceIndexToDeviceIndex(i, hostDevice);
				const PaDeviceInfo* dinfo = Pa_GetDeviceInfo(deviceNum);
				printf_device_info(deviceNum, dinfo);
				if (dinfo != NULL && dinfo->maxInputChannels > 0)
				{
					device_list[deviceNum] = dinfo;
				}
			}
			
		}
	}
	_is_init = true;
	//默认设备
	if (LoopbackAudio::select_index == -1)
	{
		//查找系统默认播放设备并设置？
		PaDeviceIndex idx = Pa_GetDefaultOutputDevice();
		const PaDeviceInfo* dinfo = Pa_GetDeviceInfo(idx);
		QString name = QString::fromStdString(dinfo->name);
		std::map<int, const PaDeviceInfo*>::iterator it = device_list.begin();
		for (; it != device_list.end(); ++it)
		{
			QString str1 = QString::fromStdString(it->second->name);
			if (str1.indexOf(name) != -1)
			{
				select_index = it->first;
				DeviceInfo di;
				di.pa_index = it->first;
				di.device = it->second;
				device_vec.push_back(di);
				break;
			}
		}
	}
	//
	std::map<int, const PaDeviceInfo*>::iterator it = device_list.begin();
	for (; it != device_list.end(); ++it)
	{
		if (it->first != select_index)
		{
			DeviceInfo di;
			di.pa_index = it->first;
			di.device = it->second;
			device_vec.push_back(di);
		}
	}
}

void LoopbackAudio::start_record(int index)
{
	qDebug() << "start loopback record, idx: " << index;
	_src_rate = 0;
	if (device_list.find(index) == device_list.end())
	{
		return;
	}
	_src_rate = device_list[index]->defaultSampleRate;

	if (_src_rate < 44100)
	{
		is_good = false;
		return;
	}
	if (_record_thread)
	{
		return;
	}
	_swr_ctx = NULL;

	_is_record_stop = false;
	_record_thread = std::make_shared<std::thread>(&LoopbackAudio::thread_record, index);
}

void LoopbackAudio::stop_record()
{
	_is_record_stop = true;
	qDebug() << "LoopbackAudio::stop_record";
	if (_stream)
	{
		qDebug() << "LoopbackAudio::stop_record, stop stream";
		Pa_AbortStream(_stream);
		Pa_StopStream(_stream);
		_stream = NULL;
		qDebug() << "LoopbackAudio::stop_record, stop stream over";
	}
	//
	if (_record_thread)
	{
		qDebug() << "LoopbackAudio::stop_record, stop thread";
		_record_thread->join();
		_record_thread = std::shared_ptr<std::thread>();
		qDebug() << "LoopbackAudio::stop_record, stop thread over";
	}
}

//char record_temp_buff[SAMPLES_PER_FRAME* PCM_CHANNEL_BIT *2];
//char swr_temp_buff[SAMPLES_PER_FRAME * 2 * 2];

static int frame_stat = 0;
static int64_t frame_time = 0;

//static int off = 0;
//static char pcm_aaa_buff[1024 * PCM_CHANNEL_BIT * 2 * 43 * 50];//50s的原始数据
//static bool aaa_is_save = false;


int record_cb(
	const void* input, void* output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
#ifdef LOOP_BACK_PRINT_TEST
	if (frame_time == 0)
	{
		frame_time = SUtil::getTimeStampMilli();
	}
	frame_stat += frameCount;
	if (frame_stat >= 44100)
	{
		int64_t now = SUtil::getTimeStampMilli();
		log_w("record cd, frame count: %d, time diff: %lld", frame_stat, now - frame_time);
		frame_time = now;
		frame_stat = 0;
	}
#endif
	//qDebug() << "loopback frameCount: " << frameCount;
	if (LoopbackAudio::_is_record_stop)
	{
		return paComplete;
	}
	int framesize = frameCount * loop_sample_size * 2;//PCM_CHANNEL_BIT
	if (framesize > LoopbackAudio::record_frame_len)
	{
		LoopbackAudio::re_init_buf(framesize);
	}
	
	//重采样
	if (LoopbackAudio::_swr_ctx != NULL)
	{
		memcpy(LoopbackAudio::record_frame_buff, input, framesize);

		//qDebug() << "swr 1, input frame: " << framesize;
		int count = LoopbackAudio::do_swr(LoopbackAudio::_swr_ctx, LoopbackAudio::record_frame_buff, frameCount, LoopbackAudio::_src_rate,
			LoopbackAudio::swr_temp_buff, LoopbackAudio::swr_temp_len, LoopbackAudio::_dst_rate);
		//qDebug() << "swr 2, count: " << count;
		LoopbackAudio::loop_bufptr->push_unspecified2(LoopbackAudio::swr_temp_buff, count * 2 * 2);
		//qDebug() << "swr 3 ";
	}
	else
	{
		LoopbackAudio::loop_bufptr->push_unspecified2((const char*)input, framesize);
		//不需要重采样，直接复制
		//LoopbackAudio::loop_bufptr->push((const char*)input, framesize);
	}

	return paContinue;
}

void LoopbackAudio::re_init_buf(int size)
{
	if (size > record_frame_len)
	{
		if (record_frame_buff)
		{
			free(record_frame_buff);
		}
		record_frame_buff = (char*)malloc(size);
		record_frame_len = size;
	}
	//
	if (size > swr_temp_len)
	{
		if (swr_temp_buff)
		{
			free(swr_temp_buff);
		}
		swr_temp_buff = (char*)malloc(size);
		swr_temp_len = size;
	}
}

void LoopbackAudio::thread_record(int index)
{
	const PaDeviceInfo* dinfo = Pa_GetDeviceInfo(index);
	if (!dinfo)
	{
		is_good = false;
		return;
	}

	PaStream* inputStream;
	PaStreamParameters inputParam;
	inputParam.channelCount = 2;
	inputParam.device = index;
	inputParam.sampleFormat = paFloat32;
	inputParam.hostApiSpecificStreamInfo = NULL;
	inputParam.suggestedLatency = dinfo->defaultHighInputLatency;//dinfo->defaultLowInputLatency;
	loop_sample_size = 4;

	PaError err = Pa_IsFormatSupported(&inputParam, NULL, dinfo->defaultSampleRate);
	if (err != paNoError)
	{
		loop_sample_size = 2;
		//log_e("port recorder, format not support, index: %d, name: %s, sampleFormat: %d, sampleRate: %d", index, dinfo->name, inputParam.sampleFormat, dinfo->defaultSampleRate);
		qDebug() << "port recorder, try 16bit";
		inputParam.sampleFormat = paInt16;
		err = Pa_IsFormatSupported(&inputParam, NULL, dinfo->defaultSampleRate);
		if (err != paNoError)
		{
			qDebug() << "port recorder, try 16bit failed.";
		}
	}
	if (err != paNoError)
	{
		qDebug() << "format not support, loopback thread stop.";
		is_good = false;
		return;
	}
	//是否需要重采样
	if (inputParam.sampleFormat == paInt16 && dinfo->defaultSampleRate == 44100)
	{
		_swr_ctx = NULL;
	}
	else
	{
		AVSampleFormat src_fmt = AV_SAMPLE_FMT_FLT;
		if (inputParam.sampleFormat == paInt16)
		{
			src_fmt = AV_SAMPLE_FMT_S16;
		}
		_swr_ctx = init_swr(_src_rate, AV_CH_LAYOUT_STEREO, src_fmt, _dst_rate, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16);
	}

	err = Pa_OpenStream(&inputStream, &inputParam, NULL, dinfo->defaultSampleRate, paFramesPerBufferUnspecified, 0, record_cb, NULL);

	if (err != paNoError)
	{
		std::cout << "open error: " << Pa_GetErrorText(err) << std::endl;
		is_good = false;
	}
	err = Pa_StartStream(inputStream);
	if (err != paNoError)
	{
		std::cout << "start error: " << Pa_GetErrorText(err) << std::endl;
		is_good = false;
	}
	_stream = inputStream;
}

struct SwrContext* LoopbackAudio::init_swr(int src_rate, int src_channels, int src_fmt, int dst_rate, int dst_channels, int dst_fmt)
{
	struct SwrContext* swr_ctx;
	/* create resampler context */
	swr_ctx = swr_alloc();
	/* set options */
	av_opt_set_int(swr_ctx, "in_channel_layout", src_channels, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", src_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", (AVSampleFormat)src_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout", dst_channels, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", dst_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", (AVSampleFormat)dst_fmt, 0);

	int ret = 0;
	/* initialize the resampling context */
	if ((ret = swr_init(swr_ctx)) < 0)
	{
		fprintf(stderr, "Failed to initialize the resampling context\n");
		return NULL;
	}
	return swr_ctx;
}

int LoopbackAudio::do_swr(struct SwrContext* swr, void* in_data, int nb_in_samples, int src_rate, void* out_data, int out_len, int dst_rate)
{
	int dst_nb_samples = av_rescale_rnd(nb_in_samples, dst_rate, src_rate, AV_ROUND_UP);
	if (dst_nb_samples * 2 * 2 > out_len)
	{
		//空间太小，无法重采样
		fprintf(stderr, "Too small buffer for ouput\n");
		return -1;
	}
	static uint8_t* out_buf[1];
	const static uint8_t* in_buf[1];
	out_buf[0] = (uint8_t*)out_data;
	in_buf[0] = (uint8_t*)in_data;
	int num = swr_convert(swr, &out_buf[0], dst_nb_samples, &in_buf[0], nb_in_samples);
	//fprintf(stderr, "dst_nb_samples:real_out_sample, %d:%d\n", dst_nb_samples, num);
	return num;
}

void LoopbackAudio::destory()
{
	Pa_Terminate();
}