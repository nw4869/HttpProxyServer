//
// Created by Night Wind on 16/2/29.
//

#ifndef HTTPPROXYSERVER_HTTPPROXYSERVER_H
#define HTTPPROXYSERVER_HTTPPROXYSERVER_H

#include <iostream>


class HttpProxyServer {

public:
    HttpProxyServer();

    ~HttpProxyServer();

    void run(const std::string &address, const int port);

private:
    static const int MAX_BUFF = 65535;
    int listenQ = 1024;


    int processClient(const int connfd);

    void processRequest(const int connfd, const char *request, int &isConnect);

    int parseAddrPort(const char *request, char *address, const size_t addrLen, int &port, int &isConnect);

    int forwardRequest(const int connfd, const char *const request, const char* address, const int port);
};


#endif //HTTPPROXYSERVER_HTTPPROXYSERVER_H
