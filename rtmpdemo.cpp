#include "rtmpdemo.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComBoBox>
#include <QCheckBox>
#include <QLabel>
#include "capture/loopback_audio.h"
#include "enc/aac_sender.h"
#include "enc/rtmp_sender.h"
#include "enc/rtmp_pusher.h"

#pragma execution_character_set("utf-8")

rtmpdemo::rtmpdemo(QWidget *parent)
    : QWidget(parent)
{
	_master = new DcLiveMaster();
	//_sender = new FileAacSender();

	//_master->setSender(_sender);

	_rtmpSender = new RtmpAacSender(new RtmpPush());
	_master->setRtmpSender(_rtmpSender);

	QLabel* lbl_addr = new QLabel(tr("推流地址："));
	QLineEdit* le_addr = new QLineEdit;
	le_addr->setPlaceholderText(QString("rtmp://"));
	_le_addr = le_addr;

	QPushButton* btn_start = new QPushButton(tr("开始推流"));
	QPushButton* btn_quit = new QPushButton(tr("停止推流"));

	QLabel* lbl_mic = new QLabel(tr("麦克风："));
	_cb_mic = new QComboBox;
	QLabel* lbl_bgm = new QLabel(tr("伴奏："));
	_cb_bgm = new QComboBox;

	connect(btn_start, &QPushButton::clicked, this, &rtmpdemo::slot_start_record);
	connect(btn_quit, &QPushButton::clicked, this, &rtmpdemo::slot_stop_record);

	QHBoxLayout* hLayout1 = new QHBoxLayout;
	QHBoxLayout* hLayout2 = new QHBoxLayout;
	QHBoxLayout* hLayout3 = new QHBoxLayout;

	hLayout1->addWidget(lbl_addr);
	hLayout1->addWidget(le_addr);

	hLayout2->addStretch();
	hLayout2->addWidget(btn_start);
	hLayout2->addSpacing(35);
	hLayout2->addWidget(btn_quit);
	hLayout2->addStretch();

	hLayout3->addStretch();
	hLayout3->addWidget(lbl_mic);
	hLayout3->addWidget(_cb_mic);
	hLayout3->addSpacing(35);
	hLayout3->addWidget(lbl_bgm);
	hLayout3->addWidget(_cb_bgm);
	hLayout3->addStretch();

	QVBoxLayout* vMainLayout = new QVBoxLayout;
	vMainLayout->addLayout(hLayout1);
	vMainLayout->addLayout(hLayout2);
	vMainLayout->addLayout(hLayout3);

	setLayout(vMainLayout);
}

void rtmpdemo::initDevice()
{
	PaError err = Pa_Initialize();
	if (err != paNoError)
	{
		return;
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
				if (dinfo != NULL && dinfo->maxInputChannels > 0)
				{
					_bgm_device_list[deviceNum] = dinfo;
					_cb_bgm->addItem(QString::fromStdString(dinfo->name), QVariant(deviceNum));
				}
			}
		}
		else if (hinfo->type == paDirectSound)
		{
			for (PaDeviceIndex hostDevice = 0; hostDevice < hinfo->deviceCount; ++hostDevice)
			{
				PaDeviceIndex deviceNum = Pa_HostApiDeviceIndexToDeviceIndex(i, hostDevice);
				const PaDeviceInfo* dinfo = Pa_GetDeviceInfo(deviceNum);
				if (dinfo != NULL && dinfo->maxInputChannels > 0)
				{
					_mic_device_list[deviceNum] = dinfo;
					_cb_mic->addItem(QString::fromStdString(dinfo->name), QVariant(deviceNum));
					//将设备保存到录音对象中
					PortRecorder::addPaDevice(deviceNum, dinfo);
				}
			}
		}
	}
}

void rtmpdemo::slot_start_record()
{
	if (_action)
	{
		return;
	}
	_action = true;
	int mic_index = _cb_mic->currentIndex();
	int bgm_index = _cb_bgm->currentIndex();

	QVariant var1 = _cb_mic->itemData(mic_index);
	QVariant var2 = _cb_bgm->itemData(bgm_index);
	int pa_mic = var1.value<int>();
	int pa_bgm = var2.value<int>();

	std::string rtmp;
	rtmp = _le_addr->text().toStdString();
	if (rtmp.size() < 8)
	{
		return;
	}

	LoopbackAudio::select_index = pa_bgm;
	_master->setPortRecorder(pa_mic);

	if (_sender)
	{
		_sender->startEncThread();
	}
	if (_rtmpSender)
	{
		_rtmpSender->startEncThread();
	}
	_master->liveOn(rtmp);
}

void rtmpdemo::slot_stop_record()
{
	if (_action)
	{
		if (_sender)
		{
			_sender->stopEncThread();
		}
		if (_rtmpSender)
		{
			_rtmpSender->stopEncThread();
		}
		_master->liveStop();
		_action = false;
	}
}
