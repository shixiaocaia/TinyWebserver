#include "./Webserver/webserver.h"




int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        perror("Usage: ./server port\n");
    }

    int port = atoi(argv[1]);

    int thread_nums = 8;
    int max_queue_nums = 10000;

    Webserver server(port, thread_nums, max_queue_nums);

    // 创建线程池
    server.CreateThreadPool();

    //监听事件
    server.ListenEvents();

    //事件循环
    server.LoopEvent();

    return 0;
}