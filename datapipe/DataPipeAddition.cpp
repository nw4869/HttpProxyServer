//
// Created by Night Wind on 16/3/4.
//

#include <string.h>
#include <iostream>
#include "DataPipeAddition.h"

int DataPipeAddition::pipe(const int dstFd) {
//    std::cout << "addition: " << addition << std::endl;
    if (addition != nullptr && len > 0)
    {
        ssize_t n = writen(dstFd, addition, len);
//        std::cout << "written: " << n << std::endl;
        delete [] addition;
        addition = nullptr;
        if (n < 0)
        {
            // 写入错误
            return -2;
        }
    }
    // 调用父类pipe
    return DefaultDataPipe::pipe(dstFd);
}

DataPipeAddition::DataPipeAddition(const int srcFd, const char *addition, const size_t len)
        : DefaultDataPipe(srcFd), len(len)
{
    this->addition = new char[len];
    strncpy(this->addition, addition, len);
}

DataPipeAddition::~DataPipeAddition()
{
    if (addition != nullptr)
    {
        delete [] addition;
        addition = nullptr;
    }
}
