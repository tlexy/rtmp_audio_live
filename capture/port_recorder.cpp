#include "port_recorder.h"
#include "iport_callback.h"
#include <QDebug>

#pragma execution_character_set("utf-8")

#define PORT_AAC_FRAME 4096
#define PORT_SAMPLES_PER_FRAME 1024
#define PORT_INPUT_TEMP (PORT_SAMPLES_PER_FRAME * 2 * 4)
//#define TEST_SAVE_DUMP

std::map<int, const PaDeviceInfo*> PortRecorder::device_list;

PortRecorder::PortRecorder(IPortCallBack* cb)
	:_cb(cb),
	_record_thread(std::shared_ptr<std::thread>()),
	_swr_ctx(NULL),
	_is_record_stop(true),
	_error_count(0),
	_sample_size(0)
{
	_record_mid = new mid_buf(1024 * 1024);

	//_target_swr_ctx = init_swr(32000, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16);
}

PortRecorder::~PortRecorder()
{
	
}

struct SwrContext* PortRecorder::init_swr(int src_rate, int src_channels, int src_fmt, int dst_rate, int dst_channels, int dst_fmt)
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

int PortRecorder::do_swr(struct SwrContext* swr, void* in_data, int nb_in_samples, int src_rate, void* out_data, int out_len, int dst_rate)
{
	int dst_nb_samples = av_rescale_rnd(nb_in_samples, dst_rate, src_rate, AV_ROUND_UP);
	if (dst_nb_samples * 2 * 2 > out_len)
	{
		//空间太小，无法重采样
		fprintf(stderr, "Too small buffer for ouput port recorder\n");
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

bool PortRecorder::startRecord(int paDeviceIndex)
{
	_src_rate = 0;
	if (device_list.find(paDeviceIndex) == device_list.end())
	{
		return false;
	}
	_src_rate = device_list[paDeviceIndex]->defaultSampleRate;
	if (_src_rate < 44100)
	{
		return false;
	}
	if (_record_thread)
	{
		return false;
	}
	//当前采样是否支持？
	_swr_ctx = NULL;

	_frame_size = 1024;
	if (_src_rate == 48000)
	{
		_frame_size = 1115;
	}
	_is_record_stop = false;
	_record_thread = std::make_shared<std::thread>(&PortRecorder::thread_record, this, paDeviceIndex);
	return true;
}

static char port_input_temp_buff[PORT_INPUT_TEMP];//最大float32
static char port_swr_buff[PORT_INPUT_TEMP*2];//最大float32

int64_t getTimeStampMilli()
{
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	auto tt = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	return tt.count();
}

static int64_t frame_time = 0;

int port_record_cb(
	const void* input, void* output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	PortRecorder* recorder = (PortRecorder*)userData;
	//qDebug() << "frameCount: " << frameCount;
	/*if (frame_time == 0)
	{
		frame_time = timeGetTime();
	}

	int64_t now = timeGetTime();
	if (now - frame_time > 60)
	{
		qDebug() << "record cd, time diff: " << now - frame_time;
	}
	frame_time = now;*/
	if (recorder->_is_record_stop)
	{
		qDebug() << "----------------------------recorder callback complete.";
		return paComplete;
	}
	int framesize = frameCount * recorder->_sample_size * 2;
	/*if (framesize > PORT_INPUT_TEMP)
	{
		framesize = PORT_INPUT_TEMP;
		if (recorder->_error_count >= 100)
		{
			recorder->_error_count = 0;
		}
	}*/
	recorder->_record_mid->push((const uint8_t*)input, framesize);
	while (recorder->_record_mid->usable_count() >= PORT_AAC_FRAME)
	{
		if (recorder->_swr_ctx != NULL)
		{
			int count = recorder->do_swr(recorder->_swr_ctx, recorder->_record_mid->data(), 1024, recorder->_src_rate,
				port_swr_buff, sizeof(port_swr_buff), recorder->_dst_rate);
			recorder->_cb->stream_cb(port_swr_buff, count, 16);
		}
		else
		{
			//不需要重采样，直接复制
			recorder->_cb->stream_cb(recorder->_record_mid->data(), 1024, 16);
		}
		recorder->_record_mid->set_read(PORT_AAC_FRAME);
	}

	//memcpy(port_input_temp_buff, input, framesize);
	//{
	//	//重采样
	//	if (recorder->_swr_ctx != NULL)
	//	{
	//		//qDebug() << "swr 1, input frame: " << framesize;
	//		int count = recorder->do_swr(recorder->_swr_ctx, port_input_temp_buff, frameCount, recorder->_src_rate,
	//			port_swr_buff, sizeof(port_swr_buff), recorder->_dst_rate);
	//		//qDebug() << "swr 2, count: " << count;
	//		recorder->_cb->stream_cb(port_swr_buff, count, 16);
	//		//qDebug() << "swr 3 ";
	//	}
	//	else
	//	{
	//		//不需要重采样，直接复制
	//		recorder->_cb->stream_cb(port_input_temp_buff, frameCount, 16);
	//	}
	//}

	return paContinue;
}

void PortRecorder::thread_record(int index)
{
	const PaDeviceInfo* dinfo = Pa_GetDeviceInfo(index);
	if (!dinfo)
	{
		return;
	}

	PaStream* inputStream;
	PaStreamParameters inputParam;
	inputParam.channelCount = 2;
	inputParam.device = index;
	inputParam.sampleFormat = paInt16;// paFloat32;

	inputParam.hostApiSpecificStreamInfo = NULL;
	inputParam.suggestedLatency = dinfo->defaultHighInputLatency;//dinfo->defaultLowInputLatency;
	if (inputParam.sampleFormat == paFloat32)
	{
		_sample_size = 4;
	}
	else
	{
		_sample_size = 2;
	}
	PaError err = Pa_IsFormatSupported(&inputParam, NULL, dinfo->defaultSampleRate);
	if (err != paNoError)
	{
		_sample_size = 2;
		/*log_e("port recorder, format not support, index: %d, name: %s, sampleFormat: %d, sampleRate: %d", index, dinfo->name, inputParam.sampleFormat, dinfo->defaultSampleRate);
		log_e("port recorder, try 16bit");*/
		inputParam.sampleFormat = paInt16;
		err = Pa_IsFormatSupported(&inputParam, NULL, dinfo->defaultSampleRate);
		if (err != paNoError)
		{
		}
	}
	if (err != paNoError)
	{
		return;
	}
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
		_src_fmt = src_fmt;
		_swr_ctx = init_swr(_src_rate, AV_CH_LAYOUT_STEREO, src_fmt, _dst_rate, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16);
	}
	//log_w("port recorder open stream: index: %d, name: %s, sampleFormat: %d, sampleRate: %f", index, dinfo->name, inputParam.sampleFormat, dinfo->defaultSampleRate);
	err = Pa_OpenStream(&inputStream, &inputParam, NULL, dinfo->defaultSampleRate, paFramesPerBufferUnspecified, 0, port_record_cb, this);

	if (err != paNoError)
	{
		//log_e("port recorder, open error: %s", Pa_GetErrorText(err));
	}
	err = Pa_StartStream(inputStream);
	if (err != paNoError)
	{
		//log_e("start error: ", Pa_GetErrorText(err));
	}
	_stream = inputStream;
}

void PortRecorder::stopRecord()
{
	qDebug() << "stopRecord...";
	_is_record_stop = true;
	if (_stream)
	{
		Pa_AbortStream(_stream);
		Pa_StopStream(_stream);
		_stream = NULL;
	}
	
	qDebug() << "stopRecord 0...";
	if (_record_thread)
	{
		_record_thread->join();
		_record_thread = std::shared_ptr<std::thread>();
	}
	qDebug() << "stopRecord 1...";
	
	qDebug() << "stopRecord 2...";
	/*if (_ns)
	{
		_ns = NULL;
	}*/
}

void PortRecorder::addPaDevice(int deviceIdx, const PaDeviceInfo* dinfo)
{
	device_list[deviceIdx] = dinfo;
}