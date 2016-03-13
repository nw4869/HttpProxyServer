//
// Created by Night Wind on 16/3/8.
//

#include "ProxyConn.h"
#include "unistd.h"

#include <iostream>
#include <string.h>
#include <algorithm>
#include <regex>

using namespace std;

ProxyConn::ProxyConn(int srcFd, int dstFd, ConnType connType, size_t maxBuff)
        : srcFd(srcFd), dstFd(dstFd), connType(connType), MAX_BUFF(maxBuff), dstReady(0)
{
    recvBuff = new char[MAX_BUFF];
    sendBuff = new char[MAX_BUFF];

    if (connType == HTTPS)
    {
        initHttpsConn();
    }
}

ProxyConn::~ProxyConn()
{
    delete [] recvBuff;
    delete [] sendBuff;
    recvBuff = nullptr;
    sendBuff = nullptr;
}

int ProxyConn::initHttpsConn()
{
    // 返回源端 HTTP/1.1 200 Connection Established
    if (strlen(RESP_CONNECT) <= MAX_BUFF)
    {
        sendLen = strlen(RESP_CONNECT);
        strncpy(sendBuff, RESP_CONNECT, sendLen);
//        std::cout << "HTTPS: sendbuf init to " << sendBuff << std::endl;
    }
    else
    {
        //buff 不够
        std::cerr << "strlen(RESP_CONNECT) > MAX_BUFF" << std::endl;
        return -1;
    }
    return 0;
}

ssize_t ProxyConn::doRead(int fd, char* buff, size_t len)
{
    ssize_t n;
    if ( (n = ::read(fd, buff, len))  > 0)
    {
        // len += n;
//        std::string s(buff, std::min((uint) *len, 256u));
//        std::cout << "read: " << s << std::endl;
        if (fd == srcFd)
        {
            if ( connType == HTTP)
            {
//                string s(buff, len);
//                regex e("(^[A-Z]{3,10} )(http://.{1,4096})(/.{0,4096}? HTTP/\\d\\.\\d\r\n)");
//                s = regex_replace(s, e, "$1$3", regex_constants::format_first_only);
//                if (len != s.size())
//                {
//                    cout << "replace to: " << s << endl;
//                    strncpy(buff, s.c_str(), len);
//                    n= s.size();
//                }
            }
            recvLen += n;
        }
        else
        {
            sendLen += n;
        }
    }
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

//     if ( (n = ::read(fd, buff + *len, MAX_BUFF - *len))  > 0)
//     {
//         *len += n;
// //        std::string s(buff, std::min((uint) *len, 256u));
// //        std::cout << "read: " << s << std::endl;
//     }
//     return n;
    return doRead(fd, buff + *len, MAX_BUFF - *len);
}

ssize_t ProxyConn::write(FdType type)
{
    ssize_t n;
    int fd = dstFd;
    size_t *len = &recvLen;
    char *buff = recvBuff;

    if (type == SRC)
    {
        fd = srcFd;
        len = &sendLen;
        buff = sendBuff;
    }

    if ( (n = ::write(fd, buff, *len)) > 0)
    {
        *len -= n;
//        std::string s(buff, std::min((uint)n, 256u));
//        std::cout << "write: " << s << std::endl;
    }
    return n;
}

void ProxyConn::closeAll()
{
    close(srcFd);
    close(dstFd);
}

size_t ProxyConn::getRecvLen() const {
    return recvLen;
}

size_t ProxyConn::getSendLen() const
{
    return sendLen;
}

int ProxyConn::isDstReady() const {
    return dstReady;
}


void ProxyConn::setDstReady(int ready)
{
    dstReady = ready;
}

ConnType ProxyConn::getConnType() const
{
    return connType;
}

void ProxyConn::setConnType(ConnType type)
{
    connType = type;
    if (connType == HTTPS)
    {
        initHttpsConn();
    }
}

int ProxyConn::getFd(FdType fdType) const {
    return fdType == SRC ? srcFd : dstFd;
}
