#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <cstdio>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "../lock/locker.h"

const int MAX_THREAD_NUMBER = 10;

class Webserver
{
public:
    Webserver(int port, int thread_nums = MAX_THREAD_NUMBER);
    ~Webserver();

public:
    void CreateThreadPool();      //创建线程池
    
public:
    int m_port;


};

#endif