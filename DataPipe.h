//
// Created by Night Wind on 16/3/2.
//

#ifndef HTTPPROXYSERVER_DATAPIPE_H
#define HTTPPROXYSERVER_DATAPIPE_H


#include <sys/types.h>

class DataPipe
{
public:
    DataPipe(const int sockfd);

    int pipe(const int destfd);

    int pipe(const int destfd, const char *addition, size_t len);

private:
    ssize_t writen(int fd, const void *vptr, size_t n);

    int sockfd;
};


#endif //HTTPPROXYSERVER_DATAPIPE_H
