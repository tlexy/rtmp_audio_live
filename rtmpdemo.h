#pragma once

#include <QtWidgets/QWidget>
#include <portaudio.h>
#include "capture/wj_live_master.h"

#pragma execution_character_set("utf-8")

class QComboBox;
class AacSender;
class RtmpAacSender;
class QLineEdit;

class rtmpdemo : public QWidget
{
    Q_OBJECT

public:
	typedef struct __DeviceInfo
	{
		int pa_index;
		const PaDeviceInfo* device;
	}DeviceInfo;
    rtmpdemo(QWidget *parent = Q_NULLPTR);

	void initDevice();

private:
	QComboBox* _cb_mic;
	QComboBox* _cb_bgm;
	DcLiveMaster* _master;
	AacSender* _sender{NULL};
	RtmpAacSender* _rtmpSender{ NULL };
	bool _action{false};

	QLineEdit* _le_addr;

	std::map<int, const PaDeviceInfo*> _mic_device_list;
	std::map<int, const PaDeviceInfo*> _bgm_device_list;

public slots:
	void slot_start_record();
	void slot_stop_record();
};
