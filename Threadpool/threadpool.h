#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <cstdio>
#include <exception>
#include <list>
#include <semaphore.h>

#include "../Lock/locker.h"
#include "../Http/httpconn.h"

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
    int m_thread_number;         // 线程的数量
    int m_max_requests;          // 最大请求数
    pthread_t *m_threads;        // 描述线程池的数组，大小为thread_number_;
    std::list<T *> m_workqueue;  // 请求队列
    Locker m_queuelocker;        // 保护请求队列的线程锁
    Sem m_queuestat;             // 用信号量表明还有多少任务未处理
    bool m_stop;                 // 判断结束线程
    int m_event_flag;            // 0 读事件， 1 写事件
};

template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests): 
m_thread_number(thread_number), m_max_requests(max_requests),
m_threads(NULL), m_stop(false)
{
    //抛出异常
    if(thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    //申请线程池数组
    m_threads = new pthread_t[thread_number];
    if(!m_threads)
        throw std::exception();

    //创建thread_number个线程，并设置线程分离
    for (int i = 0; i < thread_number; i++){
        if(pthread_create(m_threads + i, NULL, ThreadWorkFunc, this))
        {
            delete[] m_threads;
            throw std::exception();
        }

        //设置线程分离，不需要单独对工作线程进行回收
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }

}

template<typename T>
ThreadPool<T>::~ThreadPool(){
    delete m_threads;
    m_stop = true;
}


template<typename T>
bool ThreadPool<T>::Append(T *request, int event)
{
    m_event_flag = event;

    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queuelocker.lock();

    // Problem: 读写事件类型的标注

    // 判定是否超过请求队列的最大数
    if(m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 信号量提醒有任务未处理,唤醒m_queestat.wait()的线程
    m_queuestat.post();
    return true;
}

template<typename T>
void* ThreadPool<T>::ThreadWorkFunc(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;

    //线程池运行函数
    pool->ThreadRun();
    return pool;
}

template<typename T>
void ThreadPool<T>::ThreadRun()
{
    while(!m_stop)
    {
        //等待信号量
        m_queuestat.wait();
        //访问请求队列(公共区域)上锁
        m_queuelocker.lock();

        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        //取出队列前面的任务
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
        {
            continue;
        }
        
        //TODO: 默认当作proactor模式
        if(m_event_flag == 0)
        {
            if(request->ReadOnce())
            {
                request->Process();
                request->event_finish = 1;
            }
            else
            {
                //读取失败
                //request->timer_flag = 1;
                request->event_finish = 1;
            }
        }
        else if(m_event_flag == 1)
        {
            if(request->Write())
            {
                request->event_finish = 1;
            }
            else{
                //request->timer_flag = 1;
                request->event_finish = 1;
            }
        }

    }
}
#endif