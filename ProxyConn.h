//
// Created by Night Wind on 16/3/8.
//

#ifndef HTTPPROXYSERVER_PROXYCONN_H
#define HTTPPROXYSERVER_PROXYCONN_H

#include <sys/types.h>

struct ProxyConn
{
public:
    ProxyConn(const int srcFd, const int dstFd, const size_t maxBuff=8192);

    virtual ~ProxyConn();

    virtual int getSrcFd() const;

    virtual int getDstFd() const;

    virtual ssize_t readSrc();

    virtual ssize_t writeSrc();

    virtual ssize_t readDst();

    virtual ssize_t writeDst();

    virtual void closeAll();

protected:
    virtual ssize_t read(FdType type);

    virtual ssize_t write(FdType type);

public:
    static const int ERR_BUFF_FULL = -2;
    static const int ERR_BUFF_EMPTY = -3;
    const size_t MAX_BUFF;

private:
    enum FdType {UNKNOWN, SRC, DST} ;

    const int srcFd;
    const int dstFd;
    char *recvBuff;
    char *sendBuff;
    size_t recvLen;
    size_t sendLen;
};


#endif //HTTPPROXYSERVER_PROXYCONN_H
