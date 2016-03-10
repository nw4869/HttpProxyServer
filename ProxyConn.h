//
// Created by Night Wind on 16/3/8.
//

#ifndef HTTPPROXYSERVER_PROXYCONN_H
#define HTTPPROXYSERVER_PROXYCONN_H

#include <sys/types.h>
#include <string>
#include "common.h"

struct ProxyConn
{
public:
    ProxyConn(const int srcFd, const int dstFd, const ConnType connType=HTTP, const size_t maxBuff=8192);

    virtual ~ProxyConn();

    virtual int getFd(FdType fdType) const;

    virtual ssize_t read(FdType type);

    virtual ssize_t write(FdType type);

    virtual void closeAll();

    virtual size_t getRecvLen() const;

    virtual size_t getSendLen() const;

    virtual int isDstReady() const;

    virtual void setDstReady(int ready);

    virtual ConnType getConnType() const;

    virtual void setConnType(ConnType type);

protected:
    virtual int initHttpsConn();


public:
    static const int ERR_BUFF_FULL = -2;
    const size_t MAX_BUFF;

private:
    const char *RESP_CONNECT = "HTTP/1.1 200 Connection Established\r\n\r\n";

    const int srcFd;
    const int dstFd;
    char *recvBuff;
    char *sendBuff;
    size_t recvLen;
    size_t sendLen;

    int dstReady;
    ConnType connType;
};


#endif //HTTPPROXYSERVER_PROXYCONN_H
