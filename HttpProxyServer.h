//
// Created by Night Wind on 16/2/29.
//

#ifndef HTTPPROXYSERVER_HTTPPROXYSERVER_H
#define HTTPPROXYSERVER_HTTPPROXYSERVER_H

#include <string>
#include <map>
#include <set>
#include <sys/epoll.h>
#include <pthread.h>
#include "ProxyConn.h"
#include "common.h"

struct Event {
    struct epoll_event event;
    int fd;

    pthread_mutex_t *proxyConnMutexs;
//    ProxyConn* proxyConn;
    FdType fdType;
};

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

    int doRead(int fd, FdType type, pthread_mutex_t *mutex);

    int doWrite(int fd, FdType type, pthread_mutex_t *mutex);

    void handleAccept();

    void epollLoop(int listenfd);

    ProxyConn* getProxyConn(int fd) const;

    int getOtherSideFd(int fd) const;

    ProxyConn * setupFdPair(int srcFd, int dstFd);

    void close(ProxyConn *proxyConn);

    void handleEvents(struct epoll_event *events, int nfds);

    FdType getFdType(int fd) const;

    static void *hook(void* args);

    void threadMain();

    int createThreads();

    void setupProxyConnMutex();

    void handleEvent(Event event);

    void setFdDone(int fd, FdType fdType);

private:
    static const int MAX_BUFF = 8192;
    static const int MAX_EVENTS = 10;
    static const int LISTEN_Q = 1024;
    static const int MAX_THREAD = 4;
    static const int MAX_EVENT_Q = MAX_THREAD * MAX_EVENTS;

    int epfd;
    int listenfd;
    ProxyConn* fd2ProxyConnMap[LISTEN_Q+3];
    pthread_mutex_t buffMutexs[LISTEN_Q+3][2];  //[0]: readbuff, [1] sendbuff

    pthread_t threads[MAX_THREAD];

    int eventQueueHead = 0, eventQueueTail = 0;
    Event eventQueue[MAX_EVENT_Q];
    pthread_mutex_t evenMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t eventCond = PTHREAD_COND_INITIALIZER;

    std::map<int, FdType> doneFdTypeMap;
    pthread_mutex_t doneFdMapMutex;
};


#endif //HTTPPROXYSERVER_HTTPPROXYSERVER_H
