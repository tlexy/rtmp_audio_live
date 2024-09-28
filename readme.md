# 录音直播推流实验工具

## 依赖
1. [portaudio](https://github.com/PortAudio/portaudio)
2. [fdk-aac](https://github.com/mstorsjo/fdk-aac)
3. ffmpeg
4. srs-rtmp

## Usage
* 在CMakeLists中正确配置qt地址
* 在3rd目录下运行3rd_intall.bat，下载必要的依赖
* 下载ffmpeg编译版本到3rd目录下，并且在CMakeLists中正确配置ffmpeg的路径
* 使用visual studio 2022或者vs code打开