#include "webserver.h"

Webserver::Webserver(int port, int thread_nums, int max_queue_nums):
m_port(port), m_thread_nums(thread_nums), m_max_queue_nums(max_queue_nums)
{
    // Problem: 这个users定义
    users = new HttpConn[MAX_FD_NUMBER];
    //printf("complete new port\n");
}

Webserver::~Webserver()
{
    close(m_epollfd);
    close(listenfd);
    delete users;         // 删除客户端连接
    delete m_thread_pool; // 删除线程池
}

void Webserver::CreateThreadPool()
{
    m_thread_pool =new ThreadPool<HttpConn>(m_thread_nums, m_max_queue_nums);
}

void Webserver::ListenEvents()
{
    m_utils.AddSig(SIGPIPE, SIG_IGN);               //避免进程结束
    //m_utils.Addsig(SIGALRM, m_utils.SigHandler);    //设置信号捕捉函数
   //m_utils.Addsig(SIGTERM, m_utils.SigHandler);    //设置信号捕捉函数

    listenfd = socket(AF_INET, SOCK_STREAM, 0);     //监听描述符
    assert(listenfd != -1);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);    //主机字节序->网络字节序
    address.sin_port = htons(m_port);               //主机字节数->网络字节序

    int reuse_flag = 1;
    //允许重用本地地址和端口
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag));

    //socket网络通信的流程
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    //5是最大等待连接数，一般设置为5
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //这里的size没有太大意义
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    HttpConn::m_epollfd = m_epollfd;

    //将listenfd放入需要监听的内核表中
    m_utils.AddFd(m_epollfd, listenfd, false);

    //Problem:
    Utils::m_epollfd = m_epollfd;
    

}

void Webserver::LoopEvent()
{
    stop_server = false;

    while(!stop_server){
        int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
        }

        for (int i = 0; i < number; i++){
            int sockfd = m_events[i].data.fd;

            //listenfd监听到新的客户端申请
            if(sockfd == listenfd){
           
                bool flag = DealClientData();
                if(false == flag)
                    continue;
            }
            
            //异常事件
            else if(m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //epoll_ctl(Utils::m_epollfd, EPOLL_CTL_DEL, m_events[i].data.fd, 0);
                //close(m_events[i].data.fd);
                printf("监听到 sockfd %d 发生异常事件, 客户端关闭连接\n", m_events[i].data.fd);
                m_utils.RemovedFd(m_epollfd, sockfd);
                sockfd = -1;
                HttpConn::m_user_count--;
            }

            //客户端读事件
            else if(m_events[i].events & EPOLLIN){
                //printf("监听到读事件\n");
                DealWithRead(sockfd);
            }

            else if(m_events[i].events & EPOLLOUT){
                //printf("监听到写事件\n");
                DealWithWrite(sockfd);
            }
        }
    }
}

bool Webserver::DealClientData(){
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    //非阻塞listenfd,连续处理完所有客户端连接事件
    while(1){
        int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_length);
        if (connfd < 0)
        {
            printf("accept failure and the errno is %d\n", errno);
            return false;
        }

        else if(HttpConn::m_user_count >= MAX_FD_NUMBER){
            printf("Interal server busy\n");
            return false;
        }
        else{
            printf("新连接加入 %d\n", connfd);
            users[connfd].Init(connfd, client_address);
            return true;
        }
    }
}

void Webserver::DealWithRead(int sockfd){
    //TODO 定时器

    m_thread_pool->Append(&users[sockfd], 0);
    while(true)
    {
        if(1 == users[sockfd].event_finish){

            //TODO 长连接和短链接判断
            users[sockfd].event_finish = 0;
            break;
        }
    }
    
}

void Webserver::DealWithWrite(int sockfd){
    m_thread_pool->Append(&users[sockfd], 1);
    while(true){
        if(1 == users[sockfd].event_finish){

            //TODO 长连接和短链接判断
            //Problem 
            m_utils.RemovedFd(m_epollfd, sockfd);
            sockfd = -1;
            HttpConn::m_user_count--;
            users[sockfd].event_finish = 0;
            printf("响应完关闭一次\n");
            break;
        }
    }
}