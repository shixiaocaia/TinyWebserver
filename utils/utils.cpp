#include<utils.h>

int* Utils::m_pipefd = nullptr;
int Utils::m_epollfd = -1;


// Problem: 为什么要设置非阻塞
int Utils::SetNonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::AddFd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;

    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置文件操作符非阻塞
    SetNonblocking(fd);
}

void Utils::RemovedFd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void Utils::ModFd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}