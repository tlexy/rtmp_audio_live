cmake_minimum_required(VERSION 3.20)

project(rtmp_audio_live)

include(FetchContent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#set(CMAKE_WIN32_EXECUTABLE ON)

# 设置QT路径
set(QT_BIN_DIR C:/Qt/Qt5.14.2/5.14.2/msvc2017_64/bin)
# 
set(Qt5_DIR C:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5)

set(CMAKE_BUILD_TYPE Debug)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 17)

set(SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if (NOT EXISTS ${SDK_DIR}/4rd/fdk-aac/libAACenc/include/aacenc_lib.h22)
    #怎么自动下载依赖？
    #FetchContent_Declare(fdk-aac
    #GIT_REPOSITORY https://github.com/mstorsjo/fdk-aac.git)
    #message("bbb")
    #execute_process(
    #    COMMAND cd 4rd
    #    COMMAND git clone https://github.com/mstorsjo/fdk-aac.git 
    #    COMMAND cd ..)

    #https://github.com/PortAudio/portaudio
    #https://github.com/mstorsjo/fdk-aac

    #FetchContent_MakeAvailable(fdk-aac ${${SDK_DIR}/3rd})
endif()

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/out/build/${BUILD_TYPE})
message(STATUS ${CMAKE_BINARY_DIR})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/bin)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

	add_definitions(-DWIN32)
    add_definitions(-D_WIN32)
     add_definitions(-DWIN_GETPID)
	
endif()

include_directories(${SDK_DIR})
include_directories(${SDK_DIR}/3rd/librtmp)
include_directories(${SDK_DIR}/3rd/fdk-aac/libAACdec/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libAACenc/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libArithCoding/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libDRCdec/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libFDK/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libSACdec/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libSACenc/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libSBRdec/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libSBRenc/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/libSYS/include)
include_directories(${SDK_DIR}/3rd/fdk-aac/win32)
include_directories(${SDK_DIR}/3rd/portaudio/include)

include_directories(${SDK_DIR}/3rd/ffmpeg-n6.0-win64-lgpl/include)

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

link_directories(${SDK_DIR}/3rd/ffmpeg-n6.0-win64-lgpl/lib)

file(GLOB SRCS 
./*.cpp
./capture/*.cpp
./capture/*.h
./enc/*.cpp
./enc/*.h
)

set(OTHER_LIBS 
librtmp.lib
portaudio.lib
fdk-aac.lib
avcodec.lib
avformat.lib
avutil.lib
swresample.lib
)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(${PROJECT_NAME} WIN32 ${SRCS})

target_link_libraries(${PROJECT_NAME}
Qt5::Widgets
${OTHER_LIBS}
)

#set_target_properties(${PROJECT_NAME} PROPERTIES
#    WIN32_EXECUTABLE ON
#)

add_subdirectory(./3rd/librtmp)
add_subdirectory(./3rd/portaudio)
add_subdirectory(./3rd/fdk-aac)


if (EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations/qt_zh_CN.qm)
    message("file exists")
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${QT_BIN_DIR}/windeployqt ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()