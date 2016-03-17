//
// Created by Night Wind on 16/2/29.
//

#ifndef HTTPPROXYSERVER_HTTPPROXYSERVER_H
#define HTTPPROXYSERVER_HTTPPROXYSERVER_H

#include <string>
#include <map>
#include <set>
#include <sys/epoll.h>
#include "ProxyConn.h"
#include "common.h"


class HttpProxyServer {
public:
    HttpProxyServer();

    ~HttpProxyServer();

    void run(const std::string &address, const int port);

protected:

    int processClient(const int srcFd);

    int createServerSocket(const std::string &address, const int port) const;

    int createConnection(int fd, const char *address, const int port);

    int parseDestAddr(const char *line, char *destAddr, char *destPort, int &isConnect);

    void processProxy(int sourceFd, int destFd, int isConnect);

private:
    void setNonBlocking(int sockFd) const;

    int doRead(int epfd, struct epoll_event event, int fd, FdType type);

    int doWrite(int epfd, struct epoll_event event, int fd, FdType type);

    void handleAccept(int epfd, int listenfd);

    void epollLoop(int listenfd);

    ProxyConn* getProxyConn(int fd) const;

    int getOtherSideFd(int fd) const;

    ProxyConn * setupFdPair(int srcFd, int dstFd);

    void close(ProxyConn *proxyConn);

    void handleEvents(int epfd, epoll_event *events, int listenfd, int nfds);

    FdType getFdType(int fd) const;

private:
    static const int MAX_BUFF = 8192;
    static const int MAX_EVENTS = 10;
    static const int LISTEN_Q = 1024;

    int epfd;
    int listenfd;
//    std::map<int, ProxyConn*> fd2ProxyConnMap;
    ProxyConn* fd2ProxyConnMap[LISTEN_Q+3];
};


#endif //HTTPPROXYSERVER_HTTPPROXYSERVER_H
