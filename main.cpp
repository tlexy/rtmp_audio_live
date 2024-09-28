#include <iostream>
#include <QIODevice>
//#include <QApplication>
//#include <QtWidgets/QWidget>
#include <srs_librtmp.h>
#include <portaudio.h>

#include <aacenc_lib.h>

#include <windows.h>
#include "rtmpdemo.h"
#include <QApplication>

#undef main
#undef getpid

#pragma execution_character_set("utf-8")

int main(int argc, char* argv[])
{
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(1, 1), &WSAData)) {
		printf("WSAStartup failed.\n");
		return -1;
	}
	QApplication a(argc, argv);
	rtmpdemo w;
	w.initDevice();
	w.resize(600, 300);
	w.show();
	return a.exec();
}

int main1(int argc, char* argv[])
{
	QApplication a(argc, argv);

    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(1, 1), &WSAData)) {
        printf("WSAStartup failed.\n");
        return -1;
    }

    srs_human_trace("rtmp url: %s", "bbc");

	PaError err = Pa_Initialize();
	if (err != paNoError)
	{
		return -1;
	}

	Pa_Terminate();

	//
	HANDLE_AACENCODER hAacEncoder = NULL;
	AACENC_ERROR ret = aacEncOpen(&hAacEncoder, 0, 0);

	QWidget w;
	w.show();
	return a.exec();
}