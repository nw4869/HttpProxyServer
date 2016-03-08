//
// Created by Night Wind on 16/3/8.
//

#include "ProxyConn.h"
#include "unistd.h"

ProxyConn::ProxyConn(int srcFd, int dstFd, size_t maxBuff) : srcFd(srcFd), dstFd(dstFd), MAX_BUFF(maxBuff)
{
    recvBuff = new char[MAX_BUFF];
    sendBuff = new char[MAX_BUFF];
}

ProxyConn::~ProxyConn()
{
    delete [] recvBuff;
    delete [] sendBuff;
    recvBuff = nullptr;
    sendBuff = nullptr;
}

ssize_t ProxyConn::read(FdType type)
{
    ssize_t n;
    int fd = srcFd;
    size_t *len = &recvLen;
    char *buff = recvBuff;

    if (type == DST)
    {
        fd = dstFd;
        len = &sendLen;
        buff = sendBuff;
    }

    if (*len == MAX_BUFF)
    {
        return ERR_BUFF_FULL;
    }
    if ( (n = ::read(fd, buff + *len, MAX_BUFF - *len))  > 0)
    {
        *len += n;
    }
    return n;
}

ssize_t ProxyConn::readSrc()
{
    return read(SRC);
}

ssize_t ProxyConn::readDst()
{
    return read(DST);
}

ssize_t ProxyConn::write(FdType type)
{
    ssize_t n;
    int fd = dstFd;
    size_t *len = &recvLen;
    char *buff = recvBuff;

    if (type == DST)
    {
        fd = srcFd;
        len = &sendLen;
        buff = sendBuff;
    }

    if (len == 0)
    {
        return ERR_BUFF_EMPTY;
    }
    if ( (n = ::write(fd, buff, *len)) > 0)
    {
        *len -= n;
    }
    return n;
}

ssize_t ProxyConn::writeSrc()
{
    return write(SRC);
}

ssize_t ProxyConn::writeDst()
{
    return write(DST);
}

void ProxyConn::closeAll()
{
    close(srcFd);
    close(dstFd);
}

int ProxyConn::getSrcFd() const
{
    return 0;
}

int ProxyConn::getDstFd() const
{
    return 0;
}
