cmake_minimum_required(VERSION 3.20)

project(librtmp)

file(GLOB SRCS 
./*.cpp
)

add_library(${PROJECT_NAME} STATIC ${SRCS})