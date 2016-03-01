//
// Created by Night Wind on 16/2/29.
//

#include "HttpProxyServer.h"
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

int HttpProxyServer::forwardRequest(const int connfd, const char *const request, const char *address, const int port)
{
    int forwardFd;
    struct sockaddr_in forwardAddr;
    struct hostent *hptr;
    char recvBuff[MAX_BUFF];
    ssize_t nRead, nWrite;

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

    nWrite = write(forwardFd, request, strlen(request));
    cout << "wrote to forward: " << nWrite << endl;

    for (; ;)
    {
        // read
        if ((nRead = read(forwardFd, recvBuff, sizeof(recvBuff))) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        else if (nRead == 0)
        {
            cout << "EOF" << endl;
            break;      // EOF
        }

        cout << "read from forward: " << nRead << endl;
        cout << "read:" << recvBuff << endl;

        nWrite = write(connfd, recvBuff, (size_t) nRead);
        cout << "wrote to client: " << nWrite << endl;
    }

    cout << "forwarded" << endl;
    return 0;
}

int HttpProxyServer::parseAddrPort(const char *request, char *address, const size_t addrLen, int &port, int &isConnect)
{
    std::istringstream requestStream(request);
    string line, url;
    unsigned long posStart, posDiv, posEnd;

    std::getline(requestStream, line);
    cout << "first line: " << line << endl;

    if (!line.substr(0, 8).compare("CONNECT "))      //connect
    {
//        cout << "CONNECT!" << endl;
        isConnect = 1;
        posStart = 8;

        // find end pos
        if ((posEnd = line.find(" ", posStart)) == string::npos)
        {
            return -2;
        }
    }
    else
    {
//        cout << "Not Connect" << endl;

        isConnect = 0;
        // find start pos
        if ((posStart = line.find("://")) == string::npos)
        {
            return -1;
        }
        posStart += 3;

        // find end pos
        if ( (posEnd = line.find("/", posStart)) == string::npos)
        {
            return -2;
        }
    }

    url = line.substr(posStart, posEnd - posStart);
//    cout << "url: " << url << endl;
    if ((posDiv = url.find(":")) == string::npos)
    {
        // not found ':' --> use default port: 80
        port = 80;
    }
    else
    {
        port = atoi(url.substr(posDiv + 1).c_str());
    }

    strncpy(address, url.substr(0, posDiv).c_str(), addrLen);
    return 0;
}

void HttpProxyServer::processRequest(const int connfd, const char *request, int &isConnect)
{
    char address[MAX_BUFF];
    int port;
    const char *RESP_CONNECT = "HTTP/1.1 200 Connection Established\r\n\r\n";

    cout << "start process request..." << endl;
//    cout << "request:" << request << endl;

    parseAddrPort(request, address, sizeof(address), port, isConnect);
    cout << "target address: " << address << " port: " << port << endl;

//    if (!strncmp(request, "CONNECT", 7))
    if (isConnect)
    {
        // connect method is not implement
        exit(1);

//        write(connfd, RESP_CONNECT, sizeof(RESP_CONNECT));
//        return ;
    }

    forwardRequest(connfd, request, address, port);
}

int HttpProxyServer::processClient(int connfd)
{
    char ipbuff[INET_ADDRSTRLEN], recvbuff[MAX_BUFF], sendbuff[MAX_BUFF];
    struct sockaddr_in clientAddr;
    int clientPort, isConnect = 0;
    socklen_t addrLen;
    char *ptr;
    ssize_t nRead;
    size_t nLeaf;

    // get
    addrLen = sizeof(clientAddr);
    getpeername(connfd, (struct sockaddr*)&clientAddr, &addrLen);
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipbuff, sizeof(clientAddr));
    clientPort = ntohs(clientAddr.sin_port);

    cout << "new connection: " << ipbuff << ":" << clientPort << endl;

    // read request
    ptr = recvbuff;
    nLeaf = sizeof(recvbuff);
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

        // find request end flag (\r\n\r\n)
        char *requestEnd = strstr(ptr - nRead, "\r\n\r\n");
        if (requestEnd)
        {
            cout << "found request end." << endl;
            processRequest(connfd, recvbuff, isConnect);

            // clear buffer
            ptr = recvbuff;
            nLeaf = sizeof(recvbuff);
            continue;
        }
        else
        {
            cout << "not found request end" << endl;
        }
    }

    cout << "done." << endl << endl;
    return 0;
}

void HttpProxyServer::run(const string &address, const int port)
{
    int listenfd, connfd, reuse = 1;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    pid_t pid;

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
//    if (address != NULL)
    {
        if (!inet_pton(AF_INET, address.c_str(), &servaddr.sin_addr))
        {
            cerr << "listen ip address error" << endl;
            exit(1);
        }
    }
//    else
//    {
//        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
//    }

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        cerr << "bind error" << endl;
        exit(1);
    }

    cout << "listening..." << endl;
    if (listen(listenfd, this->listenQ) < 0)
    {
        cerr << "listen error" << endl;
        exit(1);
    }

    for (; ;)
    {
        len = sizeof(cliaddr);
        if ( (connfd = accept(listenfd, (struct sockaddr*)&servaddr, &len)) < 0)
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
