//
// Created by Night Wind on 16/3/4.
//

#ifndef HTTPPROXYSERVER_DATAPIPEADDITION_H
#define HTTPPROXYSERVER_DATAPIPEADDITION_H


#include "DefaultDataPipe.h"

class DataPipeAddition: public DefaultDataPipe {

public:
    DataPipeAddition(const int srcFd, const char *addition, const size_t len);

    ~DataPipeAddition();

    virtual int pipe(const int destfd);

private:
    char *addition;
    const size_t len;
};


#endif //HTTPPROXYSERVER_DATAPIPEADDITION_H
