#ifndef HTTPCONN_H
#define HTTPCONN_H


#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <string.h>
#include <stdarg.h>

#include "../utils/utils.h"

class HttpConn
{
public:
    static const int FILENAME_LEN = 200;        //设置读取文件的名称m_read_file大小
    static const int READ_BUFFER_SIZE = 2048;   //设置读缓冲区m_read_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;  //设置写缓冲区m_write_buf大小

    //报文请求的方法，本文主要就是POST，GET
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    //报文解析的结果
    enum HTTP_CODE
    {
        NO_REQUEST,         //回复NO_REQUEST继续往后处理
        GET_REQUEST,        //GET请求
        BAD_REQUEST,        //语法错误
        NO_RESOURCE,        //无请求资源
        FORBIDDEN_REQUEST,
        FILE_REQUEST,       //请求资源
        INTERNAL_ERROR,     //内部错误
        CLOSED_CONNECTION   //关闭连接
    };

    //主状态机的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,            //检查请求行
        CHECK_STATE_HEAD,                       //检查请求头
        CHECK_STATE_CONTENT                     //检查请求体
    };

    //从状态机的状态
    enum LINE_STATE
    {
        LINE_OK = 0,                            //行解析成功
        LINE_BAD,                               //语法错误
        LINE_OPEN                               //未获取到一行
    };

public:
    HttpConn() {}
    ~HttpConn() {}

public:
    void Init(int sockfd, const sockaddr_in &address);
    void CloseConn(bool real_close = true);

    bool ReadOnce();
    bool Write();

    //处理Http请求
    void Process();

    HTTP_CODE ProcessRead();
    bool ProcessWrite(HTTP_CODE ret);

    char *Getline() { return m_read_buf + m_start_line; }
    LINE_STATE ParseLine();
    HTTP_CODE ParseRequestLine(char* text);
    HTTP_CODE ParseHeader(char* text);
    HTTP_CODE ParseContent(char* text);
    HTTP_CODE DoRequest();

public:
    int m_sockfd;                                 //客户端socket
    struct sockaddr_in m_address;                 //客户端网络地址

public:
    static int m_epollfd;                         //我们还需要监视connfd的读事件,所以也需要上树
    static int m_user_count;                      //客户端总数
    //记得.h文件下初始化
    int event_finish;                             //标志事件是否完成

public:
    Utils m_utils;

private:
    void Init();
    char m_read_buf[READ_BUFFER_SIZE];    //读缓冲区
    int m_read_idx;     //缓冲区中read_buf中数据的最后一个字节的下一个位置
    int m_check_idx;    //read_buf读取的位置m_check
    int m_start_line;     //Prblem 多余内容?

    //解析请求报文中对应的变量
    char m_read_file[FILENAME_LEN]; 
    char *m_doc_root;                   //请求文件的根目录
    char *m_url;                        //请求URL
    char *m_version;                    //http版本
    char *m_host;                       //对方IP
    int m_content_length;               //请求体字节数
    bool m_linger;                      //是否长连接

    //存储发出的响应报文数据    
    char m_write_buf[WRITE_BUFFER_SIZE];  
    int m_write_idx_;           //buffer长度
    CHECK_STATE m_check_state; //主状态机状态
    METHOD m_method;           //请求方法

    struct stat m_file_stat;   //文件属性
    char *file_address_;      //内存映射区 

    struct iovec m_iv_[2];      //io向量机制iovec
    int m_iv_count;            //发送部分数
    int m_bytes_to_send;       //剩余发送字节数
    int m_bytes_have_send;     //已发送字节数
};

#endif