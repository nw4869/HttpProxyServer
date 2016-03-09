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

    if ( (n = sscanf(line, "CONNECT %255[^:]:%5s HTTP/", destAddr, destPort)) <= 0)
    {
        if ( (n = sscanf(line, "%*s %*[^:/]://%255[^:/]:%5[^:/] HTTP/", destAddr, destPort)) <= 0)
        {
            // not found
            cout << "not found: " << line << endl;
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

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, destFd, &ev) == -1) {
        perror("epoll_ctl: add");
    }

    proxyConn = setupFdPair(sourceFd, destFd);
    proxyConn->setConnType(isConnect ? ProxyConn::HTTPS : ProxyConn::HTTP);
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

    cout << "[" << ipbuff << ":" << clientPort << "] "
        << "connected!" << endl;

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
        if (( (n = createConnection(dstFd, destAddr, atoi(destPort))) < 0 && n != -3)
            ||  (n == -3 && errno != EINPROGRESS))
        {
            perror("create dst connection");
            return -3;
        }
        cout << "[" << ipbuff << ":" << clientPort << "] "
            << "destination server connected: " << destAddr << ":" << destPort << endl;

        processProxy(srcFd, dstFd, isConnect);
    }

    cout << "[" << ipbuff << ":" << clientPort << "] "
            << " done." << endl << endl;
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

void HttpProxyServer::doRead(int epfd, struct epoll_event event, int fd, FdType type)
{
    ssize_t nread, totalRead;
    struct epoll_event ev;
    ProxyConn *proxyConn = nullptr;

    cout << "doRead(" <<  epfd << ", " << fd << ")" << endl;

    if (type == UNKNOWN)
    {
        // 处理新连接
        processClient(fd);
    }
    else
    {
        proxyConn = getProxyConn(fd);
        totalRead = 0;
        if (type == SRC)
        {
            while ( (nread = proxyConn->readSrc()) > 0)
            {
                totalRead += nread;
            }
        }
        else
        {
            while ( (nread = proxyConn->readDst()) > 0)
            {
                totalRead += nread;
            }
        }
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
        else if (nread == 0 || (nread == -1 && errno != EWOULDBLOCK))
        {
            // EOF 或者 读取出错
            close(proxyConn);
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
    }

}

void HttpProxyServer::doWrite(int epfd, struct epoll_event event, int fd, FdType type)
{
    const char *RESP_CONNECT = "HTTP/1.1 200 Connection Established\r\n\r\n";
    ssize_t nwrite, totalWrite;
    struct epoll_event ev;
    ProxyConn* proxyConn = nullptr;
    int isNewConn;

    cout << "doWrite(" <<  epfd << ", " << fd << ")" << endl;

    totalWrite = 0;
    isNewConn = 0;

    proxyConn = getProxyConn(fd);
    // 处理新的对端连接响应
    if (type == DST && !proxyConn->isDstReady())
    {
        isNewConn = 1;
        if (event.events & EPOLLERR)    //连接失败
        {
            cerr << "connect dst failed" << endl;
            close(proxyConn);
            return ;
        }

        // 连接成功
        cout << "dst connect success" << endl;
        proxyConn->setDstReady(1);
        if (proxyConn->getConnType() == ProxyConn::HTTPS)
        {
            // 返回源端 HTTP/1.1 200 Connection Established
            if (::write(proxyConn->getSrcFd(), RESP_CONNECT, strlen(RESP_CONNECT)) < 0)
            {
                close(proxyConn);
                return;
            }
        }
    }
    else
    {
        // 处理已经建立连接完成的情况
        totalWrite = 0;
        if (type == SRC)
        {
            while ( (nwrite = proxyConn->writeSrc()) > 0)
            {
                totalWrite += nwrite;
            }
        }
        else
        {
            while ( (nwrite = proxyConn->writeDst()) > 0)
            {
                totalWrite += nwrite;
            }
        }

        if (nwrite == -1 && errno != EWOULDBLOCK)
        {
            // 出错
            close(proxyConn);
            return ;
        }
    }

    if (totalWrite > 0 || isNewConn)
    {
        //对端添加监听"读"
        ev.events = event.events | EPOLLIN | EPOLLET;
        ev.data.fd = getOtherSideFd(fd);
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
    ev.events = EPOLLIN;
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
    int i, fd;

    for (i = 0; i < nfds; ++i)
        {
            fd = events[i].data.fd;
            if (fd == listenfd)
            {
                handleAccept(epfd, listenfd);
                continue;
            }
            if (events[i].events & EPOLLIN)
            {
                doRead(epfd, events[i], fd, getFdType(fd));
            }
            if (events[i].events & EPOLLOUT)
            {
                doWrite(epfd, events[i], fd, getFdType(fd));
            }
        }
}

ProxyConn * HttpProxyServer::setupFdPair(int srcFd, int dstFd)
{
    ProxyConn *proxyConn = nullptr;

    proxyConn = new ProxyConn(srcFd, dstFd);
    dstFd2ProxyConnMap[dstFd] = proxyConn;
    srcFd2ProxyConnMap[srcFd] = proxyConn;
    return proxyConn;
}

void HttpProxyServer::close(ProxyConn *proxyConn)
{
    srcFd2ProxyConnMap.erase(proxyConn->getSrcFd());
    dstFd2ProxyConnMap.erase(proxyConn->getDstFd());
    // 描述中close后, 自动在epoll中移除
    proxyConn->closeAll();
    delete proxyConn;
}

int HttpProxyServer::getOtherSideFd(int fd) const
{
    FdType type;
    if ((type = getFdType(fd)) == SRC)
    {
        return srcFd2ProxyConnMap.at(fd)->getDstFd();
    }
    else if (type == DST)
    {
        return dstFd2ProxyConnMap.at(fd)->getSrcFd();
    }
    else
    {
        return -1;
    }
}

ProxyConn *HttpProxyServer::getProxyConn(int fd) const
{
    if (srcFd2ProxyConnMap.count(fd))
    {
        return srcFd2ProxyConnMap.at(fd);
    }
    else
    {
        return dstFd2ProxyConnMap.at(fd);
    }
}

HttpProxyServer::FdType HttpProxyServer::getFdType(int fd) const
{
    if (srcFd2ProxyConnMap.count(fd))
    {
        return SRC;
    }
    else if ( dstFd2ProxyConnMap.count(fd))
    {
        return DST;
    }
    else
    {
        return UNKNOWN;
    }
}
