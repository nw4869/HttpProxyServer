//
// Created by Night Wind on 16/3/2.
//

#include "DataPipe.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/errno.h>

DataPipe::DataPipe(int sockfd): sockfd(sockfd) { }

ssize_t DataPipe::writen(int fd, const void *vptr, size_t n) {
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

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}

/**
 * 将sockfd读到的数据写到destfd中,直到EOF或出错
 */
int DataPipe::pipe(const int destfd)
{
    char buff[65535];
    ssize_t nRead;

    for (; ;)
    {
        if ( (nRead = read(sockfd, buff, sizeof(buff))) < 0)
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
        if (writen(destfd, buff, (size_t) nRead) < 0)
        {
            // 写入错误
            return -2;
        }
    }

    return 0;
}

int DataPipe::pipe(const int destfd, const char *addition, size_t len) {
    if (writen(destfd, addition, len) < 0)
    {
        // 写入错误
        return -2;
    }
    return pipe(destfd);
}
