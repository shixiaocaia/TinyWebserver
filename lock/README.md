int sem_init(sem_t *sem, int pshared, unsigned int value);
- 初始化信号量
- 参数：
    - sem : 信号量变量的地址
    - pshared : 0 用在线程间 ，非0 用在进程间
    - value : 信号量中的值

int sem_destroy(sem_t *sem);
- 释放资源

int sem_wait(sem_t *sem);
- 对信号量加锁，调用一次对信号量的值-1，如果值为0，就阻塞

int sem_post(sem_t *sem);
- 对信号量解锁，调用一次对信号量的值+1，当信号量值大于0是，调用sem_wait等待信号量的线程将被唤醒。
    
//上述函数成功返回0，否则返回-1并设置errno