# 录音直播推流实验工具

## 依赖
1. [portaudio](https://github.com/PortAudio/portaudio)
2. [fdk-aac](https://github.com/mstorsjo/fdk-aac)
3. ffmpeg
4. srs-rtmp

## Usage
* 在CMakeLists中正确配置qt地址
* 在3rd目录下运行3rd_intall.bat，下载必要的依赖
* 下载ffmpeg编译版本到3rd目录下，并且在CMakeLists中正确配置ffmpeg的路径（CMakeLists.txt中的变量FFMPEG_PATH）
* 使用visual studio 2022打开
* 当出现 【无法打开文件“fdk-aac.lib”】可手动执行 cmake -S . -B build -DBUILD_SHARED_LIBS=OFF
* 在测试过程中，都是使用visual studio 2022的基础上，在有些电脑上会出现【无法打开文件“fdk-aac.lib”】的错误，这是因为fdk-aac没有生成静态的lib文件到lib输出目录导致的，有懂的朋友可以帮忙修改下CMakeLists.txt文件，确保fdk-aac这个项目一定是生成静态库
