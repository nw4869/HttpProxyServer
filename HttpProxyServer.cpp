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
    if (listen(listenfd, listenQ) < 0)
    {
        perror("listen error");
        exit(1);
    }
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

int HttpProxyServer::doRead(int epfd, struct epoll_event event, int fd, FdType type)
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
        proxyConn = getProxyConn(fd);
        totalRead = 0;
        while ( (nread = proxyConn->read(type)) > 0)
        {
            totalRead += nread;
        }
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
            // 对端添加EPOLLOUT, 下次写到目标服务器
            ev.events = event.events | EPOLLOUT | EPOLLET;
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

int HttpProxyServer::doWrite(int epfd, struct epoll_event event, int fd, FdType type)
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
        if (event.events & EPOLLERR)    //连接失败
        {
            cerr << "dst fd: " << fd << " connect failed" << endl;
            // close(proxyConn);
            return -1;
        }

        // 连接成功
        cout << "dst fd: " << fd << " connect success" << endl;
        proxyConn->setDstReady(1);
    }
    //else
    //{
    //    // 处理已经建立连接完成的情况, 
        totalWrite = 0;
        if (type == SRC)
        {
            while ( (nwrite = proxyConn->write(SRC)) > 0)
            {
                totalWrite += nwrite;
            }
        }
        else
        {
            while ( (nwrite = proxyConn->write(DST)) > 0)
            {
                totalWrite += nwrite;
            }
        }
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
        //对端添加监听"读"
        ev.events = event.events | EPOLLIN | EPOLLET;
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

void HttpProxyServer::handleAccept(int epfd, int listenfd) {
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

        handleEvents(epfd, events, listenfd, nfds);
    }
    ::close(epfd);
}

void HttpProxyServer::handleEvents(int epfd, struct epoll_event *events, int listenfd, int nfds) {
    int i, fd, occErr;
    FdType fdType;
    std::map<int, FdType> endFdTypeMap;
    for (i = 0; i < nfds; ++i)
    {
        occErr = 0;
        fd = events[i].data.fd;
//        cout << endl << "*handle fd " << fd << " begin: events = " <<  events[i].events << ", type = "
//            << getFdType(fd) << endl;
        if (fd == listenfd)
        {
//            cout << "handle accept" << endl;
            handleAccept(epfd, listenfd);
            continue;
        }

        fdType = getFdType(fd);
        if (events[i].events & EPOLLERR)
        {
            cerr << "fd: " << fd << " EPOLLERR" << endl;
            endFdTypeMap[fd] = fdType;
            occErr = 1;
        }
        if (events[i].events & EPOLLIN && !occErr && !endFdTypeMap.count(fd))
        {
//            cout << "handle read" << endl;
            if ( (occErr = doRead(epfd, events[i], fd, fdType)) < 0)
            {
                if (fdType == UNKNOWN)
                {
                    ::close(fd);
//                    cout << "close new fd: " << fd << endl;
                }
                else
                {
                    endFdTypeMap[fd] = fdType;
                }
            }
        }
        if (events[i].events & EPOLLOUT && !occErr && !endFdTypeMap.count(fd))
        {
//            cout << "handle write" << endl;
            if (doWrite(epfd, events[i], fd, fdType) < 0)
            {
                endFdTypeMap[fd] = fdType;
            }
        }
//        cout << "*handle fd " << fd << " end" << endl;
    }
    for (std::map<int, FdType>::iterator it = endFdTypeMap.begin(); it != endFdTypeMap.end(); ++it)
    {
        int fd1 = it->first, fd2;
        FdType type = it->second;
        ProxyConn *proxyConn = getProxyConn(fd1);
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
        
        if (type == SRC && !proxyConn->getRecvLen())
        {
            close(proxyConn);
            cout << endl << "close proxyConn(" << fd1 << ", " << fd2 << ")" << endl;
        }
        else if (type == DST && !proxyConn->getSendLen())
        {
            close(proxyConn);
            cout << endl << "close proxyConn(" << fd1 << ", " << fd2 << ")" << endl;
        }
    }
//    cout << "handle events end" << endl << endl;
}

ProxyConn * HttpProxyServer::setupFdPair(int srcFd, int dstFd)
{
    ProxyConn *proxyConn = nullptr;

    proxyConn = new ProxyConn(srcFd, dstFd);
//    dstFd2ProxyConnMap[dstFd] = proxyConn;
//    srcFd2ProxyConnMap[srcFd] = proxyConn;
    fd2ProxyConnMap[srcFd] = fd2ProxyConnMap[dstFd] = proxyConn;
    return proxyConn;
}

void HttpProxyServer::close(ProxyConn *proxyConn)
{
    fd2ProxyConnMap.erase(proxyConn->getFd(SRC));
    fd2ProxyConnMap.erase(proxyConn->getFd(DST));
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
    try
    {
        return fd2ProxyConnMap.at(fd);
    }
    catch (const std::out_of_range &oor)
    {
        return nullptr;
    }
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
