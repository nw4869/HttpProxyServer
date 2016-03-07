//
// Created by nw4869 on 2016/3/6.
//

#include <sys/errno.h>
#include <unistd.h>
#include "utils.h"

namespace utils
{
    ssize_t writen(int fd, const void *vptr, size_t n) {
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
        return (ssize_t) (n);
    }
}
