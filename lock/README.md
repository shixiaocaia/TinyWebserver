## 信号量

```cpp
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
```

## 互斥锁

```cpp
pthread_mutex_t m_mutex; //互斥量的类型 
    
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);
- 初始化互斥量
    - 参数 ：
    - mutex ： 需要初始化的互斥量变量
    - attr ： 互斥量相关的属性，一般设置为 NULL

int pthread_mutex_destroy(pthread_mutex_t *mutex);
- 释放互斥量的资源

int pthread_mutex_lock(pthread_mutex_t *mutex);
- 加锁，阻塞的，如果有一个线程加锁了，那么其他的线程只能阻塞等待

int pthread_mutex_trylock(pthread_mutex_t *mutex);
- 尝试加锁，如果加锁失败，不会阻塞，会直接返回。

int pthread_mutex_unlock(pthread_mutex_t *mutex);
- 解锁

```

## 条件变量
```cpp
pthread_cond_t m_cond; //条件变量的类型

int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);

int pthread_cond_destroy(pthread_cond_t *cond);

int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);
    - 等待，调用了该函数，线程会阻塞。
    - 在调用前，确保互斥锁已经加锁

int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);
    - 等待多长时间，调用了这个函数，线程会阻塞，直到指定的时间结束。

int pthread_cond_signal(pthread_cond_t *cond);
    - 唤醒一个或者多个等待的线程

int pthread_cond_broadcast(pthread_cond_t *cond);
    - 唤醒所有的等待的线程
```