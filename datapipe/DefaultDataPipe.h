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
    DefaultDataPipe(const int srcFd);

    virtual int pipe(const int dstFd);

    virtual int getSrcFd() const;

protected:
    virtual ssize_t writen(int fd, const void *vptr, size_t n);

private:
    const int srcFd;
};


#endif //HTTPPROXYSERVER_DEFAULTDATAPIPE_H
