//
// Created by Night Wind on 16/2/29.
//

#include "HttpProxyServer.h"
#include "DataPipe.h"
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sstream>
#include <netdb.h>

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

void HttpProxyServer::processProxy(int sourceFd, int destFd, int isConnect, const char *addition)
{
    pid_t pid;
    int n;
    const char *RESP_CONNECT = "HTTP/1.1 200 Connection Established\r\n\r\n";

    if ((pid = fork()) == 0)
    {
//            cout << "client fd: " << connfd << " dest fd: " << destFd << endl;
        DataPipe clientPipe(sourceFd);
        if (isConnect)
        {
            n = clientPipe.pipe(destFd);
        }
        else
        {
            n = clientPipe.pipe(destFd, addition, strlen(addition));
        }
        cout << "bye: clientPipe rnt = " << n << endl;
        exit(0);
    }

    DataPipe destPipe(destFd);
    if (isConnect)
    {
        n = destPipe.pipe(sourceFd, RESP_CONNECT, strlen(RESP_CONNECT));
    }
    else
    {
        n = destPipe.pipe(sourceFd);
    }
    cout << "bye: destPipe rnt = " << n  << endl;
//        close(destFd);
    shutdown(destFd, SHUT_RDWR);
    kill(pid, SIGKILL);
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

    cout << "new connection: " << ipbuff << ":" << clientPort << endl;

    // read request
    ptr = recvbuff;
    nLeaf = sizeof(recvbuff) - 1;
    while (nLeaf > 0)
    {
        if ( (nRead = read(connfd, ptr, nLeaf)) < 0)
        {
            if (errno == EINTR)
            {
                continue;       // call read() again
            }
            else
            {
                break;
            }
        }
        else if (nRead == 0)
        {
            break;      // EOF
        }
        cout << "read:" << nRead << endl;
//        cout << "data:" << ptr << endl;
        nLeaf -= nRead;
        ptr += nRead;
        *ptr = 0;

        if (strstr(recvbuff, "\r\n"))
        {
            if ((found = parseDestAddr(recvbuff, destAddr, destPort, isConnect)))
            {
                // found
                break;
            }
        }
    }

    if (found)
    {
        if ((destFd = createConnection(destAddr, atoi(destPort))) < 0)
        {
            return -1;
        }
        cout << "connected: " << destAddr << ":" << destPort << endl;

        processProxy(connfd, destFd, isConnect, recvbuff);
    }

    cout << "done." << endl << endl;
    return 0;
}

void HttpProxyServer::createServerSocket(const string &address, const int port, int &listenfd, sockaddr_in &servaddr) const {
    int reuse = 1;

    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Listen error" << endl;
        exit(1);
    }
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (!inet_pton(AF_INET, address.c_str(), &servaddr.sin_addr))
    {
        cerr << "listen ip address error" << endl;
        exit(1);
    }

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        cerr << "bind error" << endl;
        exit(1);
    }

    cout << "listening..." << endl;
    if (listen(listenfd, listenQ) < 0)
    {
        cerr << "listen error" << endl;
        exit(1);
    }
}

void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


void HttpProxyServer::run(const string &address, const int port)
{
    int listenfd;
    int connfd;
    socklen_t len;
    sockaddr_in servaddr, cliaddr;
    pid_t pid;
    createServerSocket(address, port, listenfd, servaddr);

    signal(SIGCHLD, sigchld_handler); // 处理僵尸进程


    for (; ;)
    {
        len = sizeof(cliaddr);
        if ( (connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len)) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                cerr << "accept error" << endl;
                exit(1);
            }
        }

        if ((pid = fork()) == 0)
        {
//            kill(0, SIGSTOP);     // for debug
            close(listenfd);
            processClient(connfd);
            close(connfd);
            exit(0);
        }
        close(connfd);
    }
    close(listenfd);
}
