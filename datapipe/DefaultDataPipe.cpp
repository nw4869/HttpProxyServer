//
// Created by Night Wind on 16/3/2.
//

#include "DefaultDataPipe.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/errno.h>
#include <iostream>

DefaultDataPipe::DefaultDataPipe(const int srcFd, const size_t maxbuff)
    : maxbuff(maxbuff), srcFd(srcFd), buff(nullptr) { }

DefaultDataPipe::~DefaultDataPipe() {
    if (buff != nullptr)
    {
        delete [] buff;
        buff = nullptr;
    }
}

ssize_t DefaultDataPipe::writen(int fd, const void *vptr, size_t n) {
    size_t		nleft;
    ssize_t		nwritten;
    const char	*ptr;

    ptr = (const char *) vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;		/* and call write() again */
            else
                return(-1);			/* error */
        }
//        std::cout << "written: " << nwritten << std::endl;

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}

/**
 * 将sockfd读到的数据写到destfd中,直到EOF或出错
 */
int DefaultDataPipe::pipe(const int dstFd)
{
    buff = new char[maxbuff];
    ssize_t nRead;
    int rtn = 0;

    for (; ;)
    {
//        std::cout << "reading..." << srcFd << std::endl;
        if ((nRead = read(srcFd, buff, maxbuff)) < 0)
        {
            if (errno == EINTR)
            {
                nRead = 0;
            }
            else
            {
                // 读取错误
                rtn = -1;
                break;
            }
        }
        else if (nRead == 0)
        {
            break;
        }
//        std::cout << "read: " << nRead << std::endl;
//        std::cout << "read: " << buff << std::endl;
        if (writen(dstFd, buff, (size_t) nRead) < 0)
        {
            // 写入错误
            rtn = -2;
            break;
        }
    }

    delete [] buff;
    buff = nullptr;
    return rtn;
}

int DefaultDataPipe::getSrcFd() const {
    return srcFd;
}
