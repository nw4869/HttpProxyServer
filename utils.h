//
// Created by nw4869 on 2016/3/6.
//

#ifndef HTTPPROXYSERVER_UTILS_H
#define HTTPPROXYSERVER_UTILS_H

#include <sys/types.h>

namespace utils
{
    ssize_t writen(int fd, const void *vptr, size_t n);
}

#endif //HTTPPROXYSERVER_UTILS_H
