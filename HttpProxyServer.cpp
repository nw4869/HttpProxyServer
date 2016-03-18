//
// Created by Night Wind on 16/2/29.
//

#include "HttpProxyServer.h"
#include "datapipe/DefaultDataPipe.h"
#include "datapipe/DataPipeAddition.h"
#include "utils.h"
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sstream>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <utility>
#include <pthread.h>

using std::string;
using std::cout;
using std::cerr;
using std::endl;

HttpProxyServer::HttpProxyServer()
{

}

HttpProxyServer::~HttpProxyServer()
{

}

int HttpProxyServer::createConnection(int fd, const char *address, const int port)
{
    int dstFd;
    struct sockaddr_in dstAddr;
    struct hostent *hptr;

//    dstFd = socket(AF_INET, SOCK_STREAM, 0);
    dstFd = fd;

    // get host
    if ((hptr = gethostbyname(address)) == NULL)
    {
        // Resolve Error
        return -1;
    }

    if (hptr->h_addrtype != AF_INET)
    {
        // unknown address type
        return -2;
    }

    bzero(&dstAddr, sizeof(dstAddr));
    dstAddr.sin_family = AF_INET;
    dstAddr.sin_port = htons(port);
    dstAddr.sin_addr.s_addr = ((struct in_addr*)(*hptr->h_addr_list))->s_addr;

    if (connect(dstFd, (struct sockaddr*)&dstAddr, sizeof(dstAddr)) < 0)
    {
        // connect error
        return -3;
    }
    return dstFd;
}

int HttpProxyServer::parseDestAddr(const char *line, char *destAddr, char *destPort, int &isConnect)
{
    int n;

    if (!line || !*line)
    {
        return 0;
    }

    if ( (n = sscanf(line, "CONNECT %255[^:]:%5s HTTP/", destAddr, destPort)) <= 0)
    {
        if ( (n = sscanf(line, "%*s %*[^:/]://%255[^:/]:%5[^:/] HTTP/", destAddr, destPort)) <= 0)
        {
            // not found
            cerr << "not found: " << line << endl;
            return 0;
        }
        else
        {
            // found not 'connect'
            if (n == 1)
            {
                strcpy(destPort, "80");
            }
            return 1;
        }
    }
    else
    {
        // found 'connect'
        if (n == 1)
        {
            strcpy(destPort, "443");
        }
        isConnect = 1;
        return 1;
    }
}

void HttpProxyServer::processProxy(int sourceFd, int destFd, int isConnect)
{
    struct epoll_event ev;
    ProxyConn *proxyConn;

    ev.events = EPOLLOUT | EPOLLET;
    ev.data.fd = destFd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, destFd, &ev) == -1) {
        perror("epoll_ctl: add");
    }

    cout << "setup fd pair: " << sourceFd << " <--> " << destFd << endl;
    proxyConn = setupFdPair(sourceFd, destFd);
    proxyConn->setConnType(isConnect ? HTTPS : HTTP);
    
    if (isConnect)
    {
        // 需要返回源端 HTTP/1.1 200 Connection Established, 所以将src监听写
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = sourceFd;
        if (epoll_ctl(epfd, EPOLL_CTL_MOD, sourceFd, &ev) == -1) {
            perror("epoll_ctl: mod");
        }
        // 取出刚才peek的，CONNECT请求
        char buff[256];
        while (read(sourceFd, buff, 256) > 0)
        {
        }
    }
}

int HttpProxyServer::processClient(int srcFd)
{
    char ipbuff[INET_ADDRSTRLEN], recvbuff[MAX_BUFF], destAddr[256], destPort[6];
    struct sockaddr_in clientAddr;
    int dstFd, clientPort, isConnect = 0, found = 0, n;
    socklen_t addrLen;
    ssize_t nRead;


    // get client info
    addrLen = sizeof(clientAddr);
    getpeername(srcFd, (struct sockaddr*)&clientAddr, &addrLen);
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipbuff, sizeof(clientAddr));
    clientPort = ntohs(clientAddr.sin_port);

//    cout << endl << "[" << ipbuff << ":" << clientPort << "] "
//        << "connected!" << endl;

    if ((nRead = recv(srcFd, recvbuff, sizeof(recvbuff) - 1, MSG_PEEK)) < 0)
    {
        perror("peek error");
        return -1;
    }

    recvbuff[nRead] = 0;
    found = parseDestAddr(recvbuff, destAddr, destPort, isConnect);

    if (found)
    {
        // if found HTTP request
        if ((dstFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("create dst socket error");
            return -2;
        }
        setNonBlocking(dstFd);
        cout << "try to connect: " << destAddr << ":" << destPort << endl;
        if (( (n = createConnection(dstFd, destAddr, atoi(destPort))) < 0 && n != -3)
            ||  (n == -3 && errno != EINPROGRESS))
        {
            perror("create dst connection");
            cerr << "createConnection() return: " << n << endl;
            ::close(dstFd);
            return -3;
        }
        // cout << "[" << ipbuff << ":" << clientPort << "] "
        //     << "destination server connected: " << destAddr << ":" << destPort << endl;

        processProxy(srcFd, dstFd, isConnect);
    }
    else
    {
        return -4;
    }

//    cout << "[" << ipbuff << ":" << clientPort << "] "
//            << " done." << endl << endl;
    return 0;
}

int HttpProxyServer::createServerSocket(const string &address, const int port) const {
    int listenfd, reuse = 1;
    struct sockaddr_in servaddr;

    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Listen error");
        exit(1);
    }
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    setNonBlocking(listenfd);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (!inet_pton(AF_INET, address.c_str(), &servaddr.sin_addr))
    {
        perror("listen ip address error");
        exit(1);
    }

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind error");
        exit(1);
    }

    return listenfd;
}

void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


void HttpProxyServer::run(const string &address, const int port)
{
    int listenfd;

    signal(SIGCHLD, sigchld_handler); // 处理僵尸进程

    listenfd = createServerSocket(address, port);
    this->listenfd = listenfd;
    cout << "listening..." << endl;
    if (listen(listenfd, LISTEN_Q) < 0)
    {
        perror("listen error");
        exit(1);
    }
    createThreads();
    setupProxyConnMutex();
    epollLoop(listenfd);
    ::close(listenfd);
}

void HttpProxyServer::setNonBlocking(int sockFd) const
{
    int opts;

    opts = fcntl(sockFd, F_GETFL);
    if(opts < 0) {
        perror("fcntl(F_GETFL)\n");
        exit(1);
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(sockFd, F_SETFL, opts) < 0) {
        perror("fcntl(F_SETFL)\n");
        exit(1);
    }

}

int HttpProxyServer::doRead(int fd, FdType type, pthread_mutex_t *mutex)
{
    ssize_t nread, totalRead;
    struct epoll_event ev;
    ProxyConn *proxyConn = nullptr;

    if (type == UNKNOWN)
    {
        // 处理新连接
//        cout << "handle new conn" << endl;
        if (processClient(fd) < 0)
        {
            // ::close(fd);
            return -1;
        }
    }
    else
    {
        pthread_mutex_lock(mutex);
        proxyConn = getProxyConn(fd);
        totalRead = 0;
        while ( (nread = proxyConn->read(type)) > 0)
        {
            totalRead += nread;
        }
        pthread_mutex_unlock(mutex);

//        cout << "total read: " << totalRead << endl;
        if (nread == ProxyConn::ERR_BUFF_FULL)
        {
            // BUFF满了,下次继续读, 由对端进行"写"后唤醒
//             去掉EPOLLIN监听
//            ev.events = event.events & ~EPOLLIN;
//            ev.data.fd = fd;
//            if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
//            {
//                perror("epoll_ctl: mod");
//            }
        }
        else if (nread == -1 && errno != EWOULDBLOCK)
        {
            // 读取出错
            perror("read");
            // close(proxyConn);
            return -1;
        }

        if (totalRead > 0)
        {
            // if (type == SRC)
            {
                cout << "read fd " << fd << " " << totalRead << endl;
            }
            // 对端添加EPOLLOUT, 下次写到目标服务器
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = getOtherSideFd(fd);
            if (epoll_ctl(epfd, EPOLL_CTL_MOD, getOtherSideFd(fd), &ev) == -1)
            {
                perror("epoll_ctl: mod");
            }
        }
        if (nread == 0)
        {
            // EOF
//            cout << "EOF" << endl;
            return -1;
        }
    }
    return 0;
}

int HttpProxyServer::doWrite(int fd, FdType type, pthread_mutex_t *mutex)
{
    ssize_t nwrite, totalWrite;
    struct epoll_event ev;
    ProxyConn* proxyConn = nullptr;
    int isNewConn;

    totalWrite = 0;
    isNewConn = 0;

    proxyConn = getProxyConn(fd);
    // 处理新的对端连接响应
    if (type == DST && !proxyConn->isDstReady())
    {
//        cout << "handle new connection result" << endl;
        isNewConn = 1;
//        if (event.events & EPOLLERR)    //连接失败
//        {
//            cerr << "dst fd: " << fd << " connect failed" << endl;
//            // close(proxyConn);
//            return -1;
//        }

        // 连接成功
        cout << "dst fd: " << fd << " connect success" << endl;
        proxyConn->setDstReady(1);
    }
    //else
    //{
    //    // 处理已经建立连接完成的情况, 
        totalWrite = 0;
        pthread_mutex_lock(mutex);
        while ( (nwrite = proxyConn->write(type)) > 0)
        {
            totalWrite += nwrite;
        }
        pthread_mutex_unlock(mutex);
//        cout << "totalWrite: "  << totalWrite << endl;
        if (nwrite == -1 && errno != EWOULDBLOCK)
        {
            // 出错
            // close(proxyConn);
            return -1;
        }
    //}

    if (totalWrite > 0 || isNewConn)
    {
        // if (type == SRC)
        {
            cout << "write fd " << fd << " " << totalWrite << endl;
        }
        //对端添加监听"读"
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = getOtherSideFd(fd);
//        cout << "otherSideFd: " << fd << " -> " <<ev.data.fd << endl;
        if (epoll_ctl(epfd, EPOLL_CTL_MOD, getOtherSideFd(fd), &ev) == -1)
        {
            perror("epoll_ctl: mod");
        }
        //自己设置监听"读"
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
        {
            perror("epoll_ctl: mod");
        }
    }
    return 0;
}

void HttpProxyServer::handleAccept() {
    struct sockaddr_in cliaddr;
    struct epoll_event ev;
    int connfd;
    socklen_t len;

    while (len = sizeof(cliaddr), (connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len)) > 0)
    {
        setNonBlocking(connfd);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = connfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1)
        {
            perror("epoll_ctl:add");
            exit(EXIT_FAILURE);
        }
//        cout << "accept new fd: " << connfd << endl;
    }
    if (connfd == -1)
    {
        if (errno != EAGAIN && errno != ECONNABORTED
            && errno != EPROTO && errno != EINTR)
            perror("accept");
    }
}

void HttpProxyServer::epollLoop(int listenfd)
{
    struct epoll_event ev, events[MAX_EVENTS];
    int epfd, nfds;

    // epoll create
    epfd = epoll_create(MAX_EVENTS);
    this->epfd = epfd;
    if (epfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    // add listenfd to epoll
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    for (; ;)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }

        handleEvents(events, nfds);
    }
    ::close(epfd);
}

void HttpProxyServer::handleEvents(struct epoll_event *events, int nfds) {
    Event event;
    ProxyConn* proxyConn;
    int fd;
    FdType type;

    for (int i = 0; i < nfds; ++i)
    {
        event.event = events[i];
        event.fd = events[i].data.fd;
        event.fdType = getFdType(event.fd);
        event.proxyConnMutexs = buffMutexs[getProxyConn(event.fd)->getFd(SRC)];

        if (events[i].events & EPOLLERR)
        {
            cerr << "fd: " << event.fd << " EPOLLERR" << endl;
            setFdDone(event.fd, event.fdType);
            continue;
        }

        handleEvent(event);
    }

    pthread_mutex_lock(&doneFdMapMutex);
    for (std::map<int, FdType>::iterator it = doneFdTypeMap.begin(); it != doneFdTypeMap.end(); ++it)
//        for (std::pair<int, FdType> &it: doneFdTypeMap)
    {
        int fd1 = it->first, fd2;
        type = it->second;
        proxyConn = getProxyConn(fd1);


        if (proxyConn)
        {
             fd2 = type == SRC ? proxyConn->getFd(DST) : proxyConn->getFd(SRC);
        }
        else
        {
            if (type == UNKNOWN)
            {
                ::close(fd1);
            }
            continue;
        }

//        cout << "try to clean fd: " << fd1 << endl;

        if (type == SRC)
        {
            pthread_mutex_lock(&buffMutexs[fd1][0]);
            if (!proxyConn->getRecvLen())
            {
                pthread_mutex_lock(&buffMutexs[fd1][1]);
                close(proxyConn);
                pthread_mutex_unlock(&buffMutexs[fd1][1]);
                cout << endl << "close proxyConn(" << fd1 << ", " << fd2 << ")" << endl;
            }
            pthread_mutex_unlock(&buffMutexs[fd1][0]);
        }

        if (type == DST)
        {
            pthread_mutex_lock(&buffMutexs[fd2][1]);
            if (!proxyConn->getRecvLen())
            {
                pthread_mutex_lock(&buffMutexs[fd2][0]);
                close(proxyConn);
                pthread_mutex_unlock(&buffMutexs[fd2][0]);
                cout << endl << "close proxyConn(" << fd1 << ", " << fd2 << ")" << endl;
            }
            pthread_mutex_unlock(&buffMutexs[fd2][1]);
        }
    }
    pthread_mutex_unlock(&doneFdMapMutex);
//    cout << "handle events end" << endl << endl;
}

ProxyConn * HttpProxyServer::setupFdPair(int srcFd, int dstFd)
{
    ProxyConn *proxyConn = nullptr;

    proxyConn = new ProxyConn(srcFd, dstFd);
    fd2ProxyConnMap[srcFd] = fd2ProxyConnMap[dstFd] = proxyConn;
    return proxyConn;
}

void HttpProxyServer::close(ProxyConn *proxyConn)
{
    fd2ProxyConnMap[proxyConn->getFd(SRC)] = nullptr;
    fd2ProxyConnMap[proxyConn->getFd(DST)] = nullptr;
    // 描述中close后, 自动在epoll中移除
    proxyConn->closeAll();
    delete proxyConn;
}

int HttpProxyServer::getOtherSideFd(int fd) const
{
    ProxyConn *pc = getProxyConn(fd);
    if (pc)
    {
        return fd == pc->getFd(SRC) ? pc->getFd(DST) : pc->getFd(SRC);
    }
    else
    {
        return UNKNOWN;
    }
}

ProxyConn *HttpProxyServer::getProxyConn(int fd) const
{
    return fd2ProxyConnMap[fd];
}

FdType HttpProxyServer::getFdType(int fd) const
{
    ProxyConn *pc = getProxyConn(fd);
    if (pc)
    {
        return fd == pc->getFd(SRC) ? SRC : DST;
    }
    else
    {
        return UNKNOWN;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void HttpProxyServer::threadMain()
{
    Event event;
    for (; ;)
    {
        // 竞争和等待事件
        pthread_mutex_lock(&evenMutex);
        while (eventQueueHead == eventQueueTail)
        {
            pthread_cond_wait(&eventCond, &evenMutex);
        }
        if (++eventQueueHead == MAX_EVENT_Q)
        {
            eventQueueHead++;
        }
        event = eventQueue[eventQueueHead];
        pthread_mutex_lock(&evenMutex);

        handleEvent(event);
    }
}
#pragma clang diagnostic pop

void HttpProxyServer::handleEvent(Event event)
{
    bool isError = false;

    // handle accept
    if (event.fd == listenfd)
    {
        handleAccept();
        return ;
    }

    if ((isError = (bool) (event.event.events & EPOLLERR)))
    {
        setFdDone(event.fd, event.fdType);
        cerr << "fd: " << event.fd << " EPOLLERR" << endl;
    }

    // handle read
    if (!isError && event.event.events & EPOLLIN)
    {
        pthread_mutex_t mutex =
                event.fdType == SRC ? event.proxyConnMutexs[0] : event.proxyConnMutexs[1];
        if ((isError = (bool) doRead(event.fd, event.fdType, &mutex)) < 0)
        {
            setFdDone(event.fd, event.fdType);
        }
    }

    // handle write
    if (!isError && event.event.events & EPOLLOUT)
    {
        pthread_mutex_t mutex =
                event.fdType == SRC ? event.proxyConnMutexs[1] : event.proxyConnMutexs[0];
        if (doWrite(event.fd, event.fdType, &mutex) < 0)
        {
            setFdDone(event.fd, event.fdType);
        }
    }

}

void HttpProxyServer::setFdDone(int fd, FdType fdType) {
    if (fdType == UNKNOWN)
    {
        ::close(fd);
    }
    else
    {
        pthread_mutex_lock(&doneFdMapMutex);
        doneFdTypeMap[fd] = fdType;
        pthread_mutex_unlock(&doneFdMapMutex);
    }
}

int HttpProxyServer::createThreads() {
    for (int i = 0; i < MAX_THREAD; i++)
    {
        if (pthread_create(&threads[i], NULL, hook, this) != 0)
        {
            perror("pthread_create");
            return 0;   // return false
        }
    }
    return 1;
}

void *HttpProxyServer::hook(void *args) {
    reinterpret_cast<HttpProxyServer*>(args)->threadMain();
    return nullptr;
}

void HttpProxyServer::setupProxyConnMutex() {
    for (int i = 3; i < LISTEN_Q + 3; i++)
    {
        buffMutexs[i][0] = buffMutexs[i][1] = PTHREAD_MUTEX_INITIALIZER;
    }
}
