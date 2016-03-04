cmake_minimum_required(VERSION 3.2)
project(HttpProxyServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp datapipe/DefaultDataPipe.cpp datapipe/DefaultDataPipe.h datapipe/DataPipeAddition.cpp datapipe/DataPipeAddition.h datapipe/DataPipe.h)
add_executable(HttpProxyServer ${SOURCE_FILES} HttpProxyServer.cpp HttpProxyServer.h)