#ifndef UTILS_H
#define UTILS_H

#include<unistd.h>
#include<fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    //设置非阻塞函数
    int SetNonblocking(int fd);

    //信号处理函数，统一信号源
    static void SigHandler(int sig);

    //设置信号函数
    void Addsig(int sig, void(handler)(int));

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void AddFd(int epollfd, int fd, bool one_shot);

    void RemovedFd(int epollfd, int fd);

    void ModFd(int epollfd, int fd, int ev);

public:
    static int *m_pipefd;    //信号的管道
    static int m_epollfd;    //epoll文件描述符
};

#endif