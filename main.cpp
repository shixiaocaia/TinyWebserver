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

    //创建线程池

    //监听事件

    //事件循环


    return 0;
}