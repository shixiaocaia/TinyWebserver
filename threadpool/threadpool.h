#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <cstdio>
#include <exception>
#include "../lock/locker.h"

template <typename T>
class ThreadPool
{
public:
    ThreadPool(int thread_number, int max_requests);
    ~ThreadPool();

    //插入任务函数
    bool Append(T *request, int event_flag);

private:
    static void *ThreadWorkFunc(void *arg);
    //设置为静态函数，通过this访问成员变量
    void ThreadRun();

private:
    int thread_number;         // 线程的数量
    int max_requests;          // 最大请求数
    pthread_t *threads;        // 描述线程池的数组，大小为thread_number_;
    std::list<T *> workqueue;  // 请求队列
    Locker queuelocker;        // 保护请求队列的线程锁
    Sem queuestat;             // 用信号量表明还有多少任务未处理
    bool stop;                 // 判断结束线程
    int event_flag             // 0 读事件， 1 写事件
};

template<typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests): 
thread_number(thread_number), max_requests(max_requests),
stop(false), threads(NULL)
{
    //抛出异常
    if(thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    //申请线程池数组
    threads = new pthread_t[thread_number];
    if(!threads)
        throw std::exception();

    //创建thread_number个线程，并设置线程分离
    for (int i = 0; i < thread_number; i++){
        if(pthread_create(thread + i, NULL, ThreadWorkFunc, this))
        {
            delete[] threads;
            throw std::exception();
        }

        //设置线程为脱离态
        if(pthread_detach(threads[i])){
            delete[] threads;
            throw std::exception();
        }
    }

}

#endif