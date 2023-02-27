#include "utils.h"

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

void Utils::SigHandler(int sig) {
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(Utils::m_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

/*
AddSig(int sig, void(handler)(int))
    向内核注册信号及其信号处理函数
*/
void Utils::AddSig(int sig, void(handler)(int)) {
    struct sigaction sa;
    bzero(&sa, sizeof(sig));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
