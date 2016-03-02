//
// Created by Night Wind on 16/2/29.
//

#ifndef HTTPPROXYSERVER_HTTPPROXYSERVER_H
#define HTTPPROXYSERVER_HTTPPROXYSERVER_H

#include <iostream>
#include <netinet/in.h>


class HttpProxyServer {

public:
    HttpProxyServer();

    ~HttpProxyServer();

    void run(const std::string &address, const int port);

private:
    static const int MAX_BUFF = 65535;
    int listenQ = 1024;


    int processClient(const int connfd);

    void createServerSocket(const std::string &address, const int port, int &listenfd, sockaddr_in &servaddr) const;

    int createConnection(const char *address, const int port);

    int parseDestAddr(const char *line, char *destAddr, char *destPort, int &isConnect);

    void processProxy(int sourceFd, int destFd, int isConnect, const char *addition);

};


#endif //HTTPPROXYSERVER_HTTPPROXYSERVER_H
