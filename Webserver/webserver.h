#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <cstdio>
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

#include "../Lock/locker.h"
#include "../Threadpool/threadpool.h"
#include "../Http/httpconn.h"
#include "../Utils/utils.h"

const int MAX_THREAD_NUMBER = 10;       //线程池中线程数量
const int MAX_EVENT_NUMBER = 10000;     //请求队列中的最大数
const int MAX_FD_NUMBER = 65536;        //最大的连接数/文件描述符数

class Webserver
{
public:
    Webserver(int port, int thread_nums = MAX_THREAD_NUMBER, int max_queue_nums = MAX_EVENT_NUMBER);
    ~Webserver();

    

//初始化部分
public:
    void CreateThreadPool();      //创建线程池
    void ListenEvents();          //开启事件循环监听
    void LoopEvent();             //开启事件循环

public:
    //事件循环针对不同事件的处理函数
    bool DealClientData();          //处理客户端连接事件
    bool DealWithSignal();          //处理信号事件
    void DealWithRead(int sockfd);  //处理读事件
    void DealWithWrite(int sockfd); //处理写事件

public:
    int m_port;                   //端口
    int m_epollfd;                //epoll句柄
    int listenfd;                 //监听描述符
    int m_pipefd[2];              //发送信号的管道
    bool stop_server;             //停止标识符
    HttpConn *users;              //各个客户端连接

    //epoll_event
    epoll_event m_events[MAX_EVENT_NUMBER];


// 线程池相关的
public:
    ThreadPool<HttpConn> *m_thread_pool;

    int m_thread_nums;            // 线程池的线程数量
    int m_max_queue_nums;         //请求队列的最大请求数

// 工具类
public:
    Utils m_utils;
};

#endif