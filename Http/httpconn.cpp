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
    m_doc_root = "/home/nowcoder/MyTinyWebserver/resources";
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

//将响应报文写入connfd
bool HttpConn::Write()
{
    if (m_bytes_to_send == 0)
    {
        m_utils.ModFd(m_epollfd, m_sockfd, EPOLLIN);
        Init();
        return true;
    }

    int temp;

    while(true){
        temp = writev(m_sockfd, m_iv_, m_iv_count);
        if(temp < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                m_utils.ModFd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            UnMap();
            return false;
        }

        m_bytes_to_send -= temp;
        m_bytes_have_send += temp;

        if((unsigned)m_bytes_to_send >= m_iv_[0].iov_len){
            m_iv_[0].iov_len = 0;
            m_iv_[1].iov_base = m_file_address + (m_bytes_have_send - m_write_idx);
            m_iv_[1].iov_len = m_bytes_to_send;
        }
        else{
            m_iv_[0].iov_base = m_write_buf + m_bytes_have_send;
            m_iv_[0].iov_len = m_iv_[0].iov_len - m_bytes_have_send;
        }

        if(m_bytes_to_send <= 0){
            UnMap();
            m_utils.ModFd(m_epollfd, m_sockfd, EPOLLIN);

            if(m_linger){
                Init();
                return true;
            }
            else{
                return false;
            }
        }
    }

}


void HttpConn::Process()
{
    //输出读取到的数据
    

    HTTP_CODE read_ret = ProcessRead();

    //没有读取完数据，修改事件状态继续读,之前的EPOLLSHOOT设置完，epollfd已经删除了该fd
    if(read_ret == NO_REQUEST){
        m_utils.ModFd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    //调用processwrite完成报文的相应，传入了读函数返回值作为判断
    bool write_ret = ProcessWrite(read_ret);
    printf("The write_buf response is\n%s\n", m_write_buf);
    //关闭访问完的连接，节省资源
    if(!write_ret){
        CloseConn();
    }

    //注册写事件，可以重复写，
    m_utils.ModFd(m_epollfd, m_sockfd, EPOLLOUT);
}

HttpConn::HTTP_CODE HttpConn::ProcessRead(){
    // LINE_STATUS 从状态机的状态
    LINE_STATE line_status = LINE_OK;
    // 报文解析结果默认是NO_REQUEST
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = ParseLine()) == LINE_OK))
    {
        //读取一行字符串，输出结果
        text = Getline();
        //printf("text: %s\n", text);
        // 更新行到开始处
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

            case CHECK_STATE_HEADER:
            {
                ret = ParseHeader(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST){
                    return DoRequest();
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
    return NO_REQUEST;
}

HttpConn::LINE_STATE HttpConn::ParseLine()
{
    char temp;

    for (; m_check_idx < m_read_idx; m_check_idx++){
        //temp为将要分析的字节
        temp = m_read_buf[m_check_idx];
        //如果当前是\r字符，则有可能会读取到完整行
        if('\r' == temp){
            //下一个字符达到了buffer结尾，则接受不完整，需要继续接受
            if((m_check_idx + 1) == m_read_idx){
                return LINE_OPEN;       
            }   
            //下一个字符是\n，将\r\n改为\0\0
            else if(m_read_buf[m_check_idx + 1] == '\n'){
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n'){
            //前一个字符是\r,则接受完整，并且第一个字符不可能是\n
            if(m_check_idx > 1 && m_read_buf[m_check_idx - 1] == '\r'){
                m_read_buf[m_check_idx - 1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //并没有找到\r\n，需要继续接受
    return LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text){
    m_url = strpbrk(text, " \t");

    if(!m_url)
        return BAD_REQUEST;

    *m_url++ = '\0';

    char *method = text;
    if( strcasecmp(method, "GET") == 0){
        m_method = GET;
    }
    else if( strcasecmp(method, "POST") == 0){
        m_method = POST;
    }
    else{
        return BAD_REQUEST;
    }

    //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符   
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    //去除前置空格
    m_version += strspn(m_version, " \t");

    //仅支持HTTP1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_version, "HTTP://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
HttpConn::HTTP_CODE HttpConn::ParseHeader(char* text){
    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection:\tkeep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    } else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } else {
        printf( "unknow header: %s\n", text);
    }
    return NO_REQUEST;    
}
HttpConn::HTTP_CODE HttpConn::ParseContent(char* text){
     //判断buffer中是否读取了消息体
    if (m_read_idx >= (m_content_length + m_check_idx))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;   
}

HttpConn::HTTP_CODE HttpConn::DoRequest(){

    //读取resources下资源
    strcpy(m_read_file, m_doc_root);
    //printf("m_read_file is %s\n", m_read_file);
    int len = strlen(m_read_file);
    //m_url指向了文件路径
    strncpy(m_read_file + len, m_url, FILENAME_LEN - len - 1);

    printf("The Http request's read file is %s\n", m_read_file);

    if(stat(m_read_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }

    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(m_read_file, O_RDONLY);

    //创建内存的映射
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

bool HttpConn::ProcessWrite(HTTP_CODE ret)
{
    switch (ret)
    {
        //内部错误，500
        case INTERNAL_ERROR:
        {
            AddStatueLine(200, error_500_title);
            AddHeaders(strlen(error_500_form));
            if (!AddContent(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            AddStatueLine(404, error_400_title);
            AddHeaders(strlen(error_400_form));
            if (!AddContent(error_400_form))
                return false;
            break;
        }
        //文件存在:200
        case FILE_REQUEST:
        {
            AddStatueLine(200, ok_200_title);
            //如果请求的资源存在
            if (m_file_stat.st_size != 0)
            {
                AddHeaders(m_file_stat.st_size);
                // 第一个iovec指针指向响应报文缓冲区，长度指向write_idx
                m_iv_[0].iov_base = m_write_buf;
                m_iv_[0].iov_len = m_write_idx;
                // 第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
                m_iv_[1].iov_base = m_file_address;
                m_iv_[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                // 发送的全部数据为响应报文头部信息和文件大小
                m_bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else {
                //如果请求的资源大小为0，则返回空白html文件
                const char *ok_string = "<html><body></body></html>";
                AddHeaders(strlen(ok_string));
                if (!AddContent(ok_string))
                    return false;            
            }
        }
        default:
            return false;
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv_[0].iov_base = m_write_buf;
    m_iv_[0].iov_len = m_write_idx;
    m_iv_count = 1;
    m_bytes_to_send = m_write_idx;
    return true;
}

void HttpConn::UnMap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool HttpConn::AddResponse(const char* format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;

    //初始化，第二个参数为最后一个确定的形参
    va_start(arg_list, format);

    //将数据写入缓冲区中，并返回写入的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    m_write_idx += len;

    //清理工作
    va_end(arg_list);
    return true;
}

bool HttpConn::AddStatueLine(int status, const char *title)
{
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::AddHeaders(int content_len)
{
    return AddContentLength(content_len) && AddLinger() && AddBlankLine();
}

bool HttpConn::AddContentLength(int content_len){
    return AddResponse("Content-Length:%d\r\n", content_len);
}

bool HttpConn::AddLinger(){
    return AddResponse("Connection:%s\r\n", (true == m_linger) ? "keep-alive" : "close");
}

bool HttpConn::AddBlankLine()
{
    return AddResponse("%s", "\r\n");
}

bool HttpConn::AddContent(const char *content){
    return AddResponse("%s", content);
}

void HttpConn::Init()
{       
    //timer_flag_ = 0;
    event_finish = 0;

    //分析报文行所需要数据
    m_read_idx = 0;      //已读数据的下一位
    m_check_idx = 0;   //当前已检查数据
    m_start_line = 0;    //当前行位置
    
    m_write_idx = 0;     //修改
    
    //主状态机及其分析对应变量
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = 0;
    m_host = 0;
    m_version = 0;
    m_content_length = 0;    
    m_linger = false;

    // 对读、写、文件名缓冲区初始化为'\0'
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_read_file, '\0', FILENAME_LEN);
}
