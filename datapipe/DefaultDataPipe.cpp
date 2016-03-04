//
// Created by Night Wind on 16/3/2.
//

#include "DefaultDataPipe.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/errno.h>
#include <iostream>

DefaultDataPipe::DefaultDataPipe(int srcFd): srcFd(srcFd) { }

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
    char buff[65535];
    ssize_t nRead;

    for (; ;)
    {
//        std::cout << "reading..." << srcFd << std::endl;
        if ((nRead = read(srcFd, buff, sizeof(buff))) < 0)
        {
            if (errno == EINTR)
            {
                nRead = 0;
            }
            else
            {
                // 读取错误
                return -1;
            }
        }
        else if (nRead == 0)
        {
            break;
        }
//        std::cout << "read: " << nRead << std::endl;
        if (writen(dstFd, buff, (size_t) nRead) != nRead)
        {
            // 写入错误
            return -2;
        }
    }

    return 0;
}

int DefaultDataPipe::getSrcFd() const {
    return srcFd;
}
