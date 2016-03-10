//
// Created by Night Wind on 16/3/8.
//

#ifndef HTTPPROXYSERVER_PROXYCONN_H
#define HTTPPROXYSERVER_PROXYCONN_H

#include <sys/types.h>

struct ProxyConn
{
public:
    enum FdType {UNKNOWN, SRC, DST};
    enum ConnType {HTTP, HTTPS};

public:
    ProxyConn(const int srcFd, const int dstFd, const ConnType connType=HTTP, const size_t maxBuff=8192);

    virtual ~ProxyConn();

    virtual int getSrcFd() const;

    virtual int getDstFd() const;

    virtual ssize_t readSrc();

    virtual ssize_t writeSrc();

    virtual ssize_t readDst();

    virtual ssize_t writeDst();

    virtual void closeAll();

    virtual size_t getRecvLen() const;

    virtual size_t getSendLen() const;

    virtual int isDstReady() const;

    virtual void setDstReady(int ready);

    virtual ConnType getConnType() const;

    virtual void setConnType(ConnType type);

protected:
    virtual ssize_t read(FdType type);

    virtual ssize_t write(FdType type);
    
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
