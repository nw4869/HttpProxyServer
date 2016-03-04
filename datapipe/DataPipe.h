//
// Created by Night Wind on 16/3/4.
//

#ifndef HTTPPROXYSERVER_DATAPIPE_H
#define HTTPPROXYSERVER_DATAPIPE_H

class DataPipe
{
public:
    virtual ~DataPipe() {};

    virtual int pipe(const int dstFd) = 0;

    //virtual int getSrcFd() const = 0;
};

#endif //HTTPPROXYSERVER_DATAPIPE_H
