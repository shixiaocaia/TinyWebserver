#include "httpconn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

int HttpConn::m_user_count = 0;     //初始化用户数为0
int HttpConn::m_epollfd = -1;       //初始化epollfd -1


//初始化地址
void HttpConn::Init(int sockfd, const sockaddr_in& address)
{
    //初始化资源目录
    m_doc_root = "";
    //初始化连接的套接字和连接地址
    m_sockfd = sockfd;
    m_address = address;

    //向epollfd添加监听事件，oneshot事件保证单个线程负责
    m_utils.AddFd(m_epollfd, m_sockfd, true);
    m_user_count++;

    //private 版本初始化private成员变量
    Init();
}

void HttpConn::CloseConn(bool real_close){
    if(m_sockfd)
    {
        printf("the write_ret is false close %d\n", m_sockfd);
        m_utils.RemovedFd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}
//读报文
bool HttpConn::ReadOnce()
{
    //如果读取数据大于缓冲区大小，返回false，这里是proactor模式
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            //非阻塞ET模式下，需要一次性读完数据，下次不会再通知所以循环读完
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            return false;
        }

        //客户端关闭连接
        else if(bytes_read == 0){
            return false;
        }
        //更新读取的缓冲区的位置
        m_read_idx += bytes_read;
    }
    return true;
}

//写报文
bool HttpConn::Write()
{

}


void HttpConn::Process()
{
    //输出读取到的数据
    printf("The HTTP request is \n%s", m_read_buf);

    HTTP_CODE read_ret = ProcessRead();

    //没有读取完数据，修改事件状态继续读
    if(read_ret == NO_REQUEST){
        m_utils.ModFd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    //调用processwrite完成报文的相应，传入了读函数返回值作为判断
    //bool write_ret = ProcessWrite(read_ret);
    printf("The write_buf response is not supported now\n");
    //关闭访问完的连接，节省资源
    CloseConn();
}

HttpConn::HTTP_CODE HttpConn::ProcessRead(){
    // LINE_STATUS 从状态机的状态
    LINE_STATE line_status = LINE_OK;
    // 报文解析结果默认是NO_REQUEST
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||((line_status == ParseLine()) == LINE_OK)){
        //读取一行字符串，输出结果
        text = Getline();

        //更新行到开始处
        m_start_line = m_check_idx;

        //主状态机
        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = ParseRequestLine(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEAD:
            {
                ret = ParseHeader(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST){
                    return GET_REQUEST;
                }
                break;
            }

            //POST
            case CHECK_STATE_CONTENT:
            {
                ret = ParseContent(text);
                if(ret == GET_REQUEST){
                    return DoRequest();
                }
                line_status = LINE_OPEN;
                break;
            }
        }

    }
}

