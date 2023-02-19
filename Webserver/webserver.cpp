#include "webserver.h"

Webserver::Webserver(int port, int thread_nums, int max_queue_nums):
m_port(port), m_thread_nums(thread_nums), m_max_queue_nums(max_queue_nums)
{
    // Problem: 这个users定义
    users = new HttpConn[MAX_FD_NUMBER];

}

Webserver::~Webserver()
{

    delete users;         // 删除客户端连接
    delete m_thread_pool; // 删除线程池
}

void Webserver::CreateThreadPool()
{
    m_thread_pool =new ThreadPool<HttpConn>(m_thread_nums, m_max_queue_nums);
}

void Webserver::ListenEvents()
{
    m_utils.Addsig(SIGPIPE, SIG_IGN);               //避免进程结束
    m_utils.Addsig(SIGALRM, m_utils.SigHandler);    //设置信号捕捉函数
    m_utils.Addsig(SIGTERM, m_utils.SigHandler);    //设置信号捕捉函数

    listenfd = socket(AF_INET, SOCK_STREAM, 0);     //监听描述符
}

void Webserver::LoopEvent()
{

}