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

int HttpProxyServer::createConnection(const char *address, const int port)
{
    int forwardFd;
    struct sockaddr_in forwardAddr;
    struct hostent *hptr;

    forwardFd = socket(AF_INET, SOCK_STREAM, 0);

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

    bzero(&forwardAddr, sizeof(forwardAddr));
    forwardAddr.sin_family = AF_INET;
    forwardAddr.sin_port = htons(port);
    forwardAddr.sin_addr.s_addr = ((struct in_addr*)(*hptr->h_addr_list))->s_addr;

    if (connect(forwardFd, (struct sockaddr*)&forwardAddr, sizeof(forwardAddr)) < 0)
    {
        // connect error
        return -3;
    }
    return forwardFd;
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
    // TODO 完成非阻塞
    char buff[MAX_BUFF];
    pid_t pid;
    int n;
    const char *RESP_CONNECT = "HTTP/1.1 200 Connection Established\r\n\r\n";

    signal(SIGPIPE, SIG_IGN);

    if ((pid = fork()) == 0)
    {
        // 转发客户端数据到服务端
//            cout << "client fd: " << connfd << " dest fd: " << destFd << endl;
        DefaultDataPipe clientPipe = DefaultDataPipe(sourceFd);
        n = clientPipe.pipe(destFd);
        cout << "bye: clientPipe rnt = " << n << endl;

        // 若目标客户端关闭, 则关闭客户端读,服务器写
        shutdown(sourceFd, SHUT_RD);
        shutdown(destFd, SHUT_WR);
        exit(0);
    }

    // 转发服务端数据到客户端
    DefaultDataPipe destPipe = DefaultDataPipe(destFd);
    if (isConnect)
    {
        read(sourceFd, buff, sizeof(buff));
        utils::writen(sourceFd, RESP_CONNECT, strlen(RESP_CONNECT));
    }
    n = destPipe.pipe(sourceFd);
    cout << "bye: destPipe rnt = " << n  << endl;

    // 若服务端关闭, 则关闭目标服务端, 客户端写, 结束该子进程
    shutdown(sourceFd, SHUT_WR);
    shutdown(destFd, SHUT_RDWR);
    kill(pid, SIGTERM);
}

int HttpProxyServer::processClient(int connfd)
{
    char ipbuff[INET_ADDRSTRLEN], recvbuff[MAX_BUFF], destAddr[256], destPort[6];
    struct sockaddr_in clientAddr;
    int destFd, clientPort, isConnect = 0, found = 0;
    socklen_t addrLen;
    char *ptr;
    ssize_t nRead;
    size_t nLeaf;
    pid_t pid;

    // get client info
    addrLen = sizeof(clientAddr);
    getpeername(connfd, (struct sockaddr*)&clientAddr, &addrLen);
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipbuff, sizeof(clientAddr));
    clientPort = ntohs(clientAddr.sin_port);

    cout << "[" << ipbuff << ":" << clientPort << "] "
        << "connected!" << endl;

    if ( (nRead = recv(connfd, recvbuff, sizeof(recvbuff) - 1, MSG_PEEK)) < 0)
    {
        perror("peek error");
        return -1;
    }

    recvbuff[nRead] = 0;
    found = parseDestAddr(recvbuff, destAddr, destPort, isConnect);

    // TODO 完成非阻塞connect
    if (found)
    {
        // if found HTTP request
        if ((destFd = createConnection(destAddr, atoi(destPort))) < 0)
        {
            return -1;
        }
        cout << "[" << ipbuff << ":" << clientPort << "] "
            << "destination server connected: " << destAddr << ":" << destPort << endl;

        processProxy(connfd, destFd, isConnect);
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

void HttpProxyServer::readAndNotifyOtherSide(int epfd, struct epoll_event event, int fd,
                                             HttpProxyServer::FdType type) {
    ssize_t nread;
    struct epoll_event ev;
    ProxyConn *proxyConn = nullptr;
    ssize_t (*readFunc)();

    proxyConn = getProxyConn(fd);
    if (type == SRC)
    {
         readFunc = proxyConn->readSrc;
    }
    else
    {
        readFunc = proxyConn->readDst;
    }
    while ( (nread = readFunc()) > 0)
    {
    }
    if (nread == ProxyConn::ERR_BUFF_FULL)
    {
        // BUFF 满了,下次继续读
        ev.events = event.events | EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
        {
            perror("epoll_ctl: mod");
        }
    }
    else if (nread == 0 || (nread == -1 && errno != EWOULDBLOCK))
    {
        // EOF 或者 读取出错
        close(proxyConn);
    }

    // 添加EPOLLOUT, 下次写到目标服务器
    ev.events = event.events | EPOLLOUT;
    ev.data.fd = getOtherSideFd(fd);
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, getOtherSideFd(fd), &ev) == -1)
    {
        perror("epoll_ctl: mod");
    }
}

void HttpProxyServer::doRead(int epfd, struct epoll_event event, int fd, FdType type)
{
    cout << "doRead(" <<  epfd << ", " << fd << ")" << endl;

    if (type == UNKNOWN)
    {
        processClient(fd);
    }
    else
    {
        readAndNotifyOtherSide(epfd, event, fd, type);
    }

}

void HttpProxyServer::doWrite(int epfd, struct epoll_event event, int fd, FdType type)
{
    char buff[MAX_BUFF];
    ssize_t len, nwrite, totalWrite;

    cout << "doWrite(" <<  epfd << ", " << fd << ")" << endl;

    // TODO 完成doWrite函数

    snprintf(buff, MAX_BUFF, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);
    len = strlen(buff);
    totalWrite = 0;
    while (totalWrite < len)
    {
        nwrite = write(fd, buff + totalWrite, (size_t) (len - totalWrite));
        if (nwrite != len - totalWrite)
        {
            if (nwrite == -1 && errno != EWOULDBLOCK)
            {
                perror("write error");
            }
            break;
        }
        totalWrite += nwrite;
    }
    ::close(fd);
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
    int epfd, nfds, fd, i;

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
                doRead(epfd, events[i], fd, fdTypeMap[fd]);
            }
            if (events[i].events & EPOLLOUT)
            {
                doWrite(epfd, events[i], fd, fdTypeMap[fd]);
            }
        }
}

void HttpProxyServer::setupFdPair(int srcFd, int dstFd)
{
    fdTypeMap[srcFd] = SRC;
    fdTypeMap[dstFd] = DST;
    src2DstMap[srcFd] = dstFd;
    dst2SrcMap[dstFd] = srcFd;
    srcFd2ProxyConnMap[srcFd] = new ProxyConn(srcFd, dstFd);
}

void HttpProxyServer::close(ProxyConn *proxyConn)
{
    fdTypeMap.erase(proxyConn->getSrcFd());
    fdTypeMap.erase(proxyConn->getDstFd());
    src2DstMap.erase(proxyConn->getSrcFd());
    dst2SrcMap.erase(proxyConn->getDstFd());
    srcFd2ProxyConnMap.erase(proxyConn->getSrcFd());
    // 描述中close后, 自动在epoll中移除
    proxyConn->closeAll();
    delete proxyConn;
}

int HttpProxyServer::getOtherSideFd(int fd) const
{
    FdType type;
    if ((type = fdTypeMap[fd]) == SRC)
    {
        return src2DstMap[fd];
    }
    else if (type == DST)
    {
        return dst2SrcMap[fd];
    }
    else
    {
        return -1;
    }
}

ProxyConn *HttpProxyServer::getProxyConn(int fd)
{
    FdType type;
    if ((type = fdTypeMap[fd]) == SRC)
    {
        return srcFd2ProxyConnMap[fd];
    }
    else if (type == DST)
    {
        return srcFd2ProxyConnMap[getOtherSideFd(fd)];
    }
    else
    {
        return nullptr;
    }
}
