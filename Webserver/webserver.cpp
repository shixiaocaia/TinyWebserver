#include "webserver.h"

Webserver::Webserver(int port, int thread_nums, int max_queue_nums):
m_port(port), m_thread_nums(thread_nums), m_max_queue_nums(max_queue_nums)
{


}

Webserver::~Webserver()
{


    delete m_thread_pool;              //删除线程池 
}

void Webserver::CreateThreadPool()
{
    m_thread_pool =new ThreadPool<HttpConn>(m_thread_nums, m_max_queue_nums);
}

