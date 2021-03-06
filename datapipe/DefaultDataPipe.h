//
// Created by Night Wind on 16/3/2.
//

#ifndef HTTPPROXYSERVER_DEFAULTDATAPIPE_H
#define HTTPPROXYSERVER_DEFAULTDATAPIPE_H

#include "DataPipe.h"
#include <sys/types.h>


class DefaultDataPipe: public DataPipe
{
public:
    DefaultDataPipe(const int srcFd, const size_t maxbuff=65535);

    ~DefaultDataPipe();

    virtual int pipe(const int dstFd);

    virtual int getSrcFd() const;

protected:
    virtual ssize_t writen(int fd, const void *vptr, size_t n);

private:
    const int srcFd;
    const size_t maxbuff;
    char *buff;
};


#endif //HTTPPROXYSERVER_DEFAULTDATAPIPE_H
