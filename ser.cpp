#include "ser.h"

/**
 * RAII 风格，完成
 */
Ser::Ser() {
    //INADDR_ANY 0.0.0.0
    //Ser("0.0.0.0", SER_PORT);
    if ((serverFD = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        ERR_EXIT("socket");

    int on = 1;
    if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) < 0) {
        ERR_EXIT("setsockopt");
    }


    m_addr_len = sizeof(m_local_addr);
    bzero(&m_local_addr, m_addr_len);

    m_local_addr.sin_family = AF_INET;
    m_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_local_addr.sin_port = htons(SER_PORT);

    Bind(serverFD, &m_local_addr);
    Listen(serverFD, LISTENQ);

    if ((m_epoll_fd = epoll_create1(0)) < 0)
        ERR_EXIT("epoll_create1");
    add_event(serverFD, EPOLLIN);

    epoll_event one{};
    //    bzero(&one, sizeof(one));  使用CPP风格的初始化，默认是零值初始化，不需要这一步
    m_epoll_event.push_back(one); //为什么这时候要先取一个出来放进去？？？
}

Ser::Ser(const char *ip, unsigned int port) {
    if ((serverFD = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        ERR_EXIT("socket");

    int on = 1;
    if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) < 0)
        ERR_EXIT("setsockopt");


    m_addr_len = sizeof(m_local_addr);
    bzero(&m_local_addr, m_addr_len);

    m_local_addr.sin_family = AF_INET;
    m_local_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &m_local_addr.sin_addr) < 0)
        ERR_EXIT("inet_pton");

    Bind(serverFD, &m_local_addr);
    Listen(serverFD, LISTENQ);

    if ((m_epoll_fd = epoll_create1(0)) < 0)
        ERR_EXIT("epoll_create1");
    add_event(serverFD, EPOLLIN);

    epoll_event one{};
//    bzero(&one, sizeof(one));  使用CPP风格的初始化，默认是零值初始化，不需要这一步
    m_epoll_event.push_back(one); //为什么这时候要先取一个出来放进去？？？
}

void Ser::Bind(int sockfd, struct sockaddr_in *addr) {
    if (bind(sockfd, (SA *) addr, m_addr_len) < 0)
        ERR_EXIT("bind");
}

void Ser::Listen(int sockfd, unsigned int num) {
    if (listen(sockfd, num) < 0)
        ERR_EXIT("listen");
}

int Ser::wait_event() {
    int ret = epoll_wait(this->m_epoll_fd, &m_epoll_event[0], m_epoll_event.size(), -1);
    if (ret < 0) {
        ERR_EXIT("epoll_wait");;
    }
    return ret;
}

/**
 *
 * @param fd socket文件描述符
 * @param state 关心的事件类型
 */
void Ser::add_event(int fd, int state) {
    epoll_event ev{};
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void Ser::delete_event(int fd, int state) {
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}

void Ser::modify_event(int fd, int state) {
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void Ser::do_accept() {
    //正如accept注释里描述的那样，每有一个新client，server都要有一个的scoket文件与之client进行数据传输
    int clientFD = accept(serverFD, nullptr, nullptr);

    if (clientFD == -1) {
        cerr << "accept err" << endl;
        return;
    }
    cout << "new fd:" << clientFD << endl;
    /*
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, len);
    getsockname(m_listenfd, (SA*)&addr, &len);
    cout << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " :: ";

    bzero(&addr, len);
    getpeername(m_listenfd, (SA*)&addr, &len);
    cout << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << endl;

    bzero(&addr, len);
    getsockname(fd, (SA*)&addr, &len);
    cout << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " :: ";
    bzero(&addr, len);
    getpeername(fd, (SA*)&addr, &len);
    cout << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << endl;
    //*/

    clientFD_vector.push_back(clientFD);//添加到已连接队列
    add_event(clientFD, EPOLLIN);//添加到epoll监听

    epoll_event ev{};
    m_epoll_event.push_back(ev);//扩大事件处理队列容量

    //do_in(fd);//启动线程处理此请求事件
}

void Ser::do_in(int fd) {
    char http_head_char[HEADSIZE];
    //todo 应该使用array，因为自带越界检查，纯array很容易误操作array之外的内存，尤其是char型的，末尾有'\0'
    std::array<char, HEADSIZE> hear_array{};

    //读报文头
    unsigned headLen{0};//严格使用CPP风格的初始化方式

    if ((headLen = readline(fd, http_head_char, HEADSIZE)) == 0) {
        do_close(fd);
        return;
    }
    http_head_char[headLen] = 0;
    //read返回0则链接断开,处理已链接列表，处理epoll监听队列,减小数组大小
    //解析存储
    char way[16] = {0};
    char uri[1024] = {0};
    char version[16] = {0};

    cout << http_head_char;

    sscanf(http_head_char, "%s %s %s\r\n", way, uri, version);
    upchar(way, sizeof(way));//统一大小写
    downchar(uri, sizeof(uri));
    upchar(version, sizeof(version));

    if (strcmp(way, "GET") == 0)//get方法请求
    {
        int ret;
        char bodybuf[BODYSIZE] = {0};
        while (1)//直到请求头结束
        {
            ret = readline(fd, bodybuf, BODYSIZE);
            if (ret == 0) {
                do_close(fd);
                return;
            }
            if ((ret == 2) && (bodybuf[0] == '\r') && (bodybuf[1] == '\n'))
                break;
            //cout << bodybuf;
            bzero(bodybuf, BODYSIZE);
        }
        do_get(fd, uri, sizeof(uri));//解析数据并完成发送任务
    } else if (strcmp(way, "POST") == 0)//post方法请求
    {
        char bodybuf[BODYSIZE] = {0};
        char keybuf[32] = {0};
        char valuebuf[512] = {0};
        size_t textlen = 0;
        int ret;
        while (1)//直到请求头结束
        {
            ret = readline(fd, bodybuf, BODYSIZE);
            if (ret == 0) {
                do_close(fd);
                return;
            }
            if ((ret == 2) && (bodybuf[0] == '\r') && (bodybuf[1] == '\n'))
                break;

            cout << bodybuf;

            downchar(bodybuf, BODYSIZE);//统一转为小写
            bzero(keybuf, sizeof(keybuf));
            bzero(valuebuf, sizeof(valuebuf));
            sscanf(bodybuf, "%s: %s\r\n", keybuf, valuebuf);//分解键值对
            if (strcmp(keybuf, "content-length") == 0) {
                sscanf(valuebuf, "%lu", &textlen);//读取文本长度
            }
            bzero(bodybuf, BODYSIZE);
        }

        char *ptext = new char[textlen + 1];
        bzero(ptext, textlen + 1);
        if (readn(fd, ptext, textlen) == 0)//读取文本
        {
            delete[] ptext;
            do_close(fd);
            return;
        }
        ptext[textlen] = '\0';
        do_post(fd, uri, sizeof(uri), ptext, textlen + 1);//解析数据并完成发送任务
        delete[] ptext;
    } else {
        cout << "Further Support." << endl;
        return;
    }
    return;
}

void Ser::do_close(int fd) {
    //重构思路：应该使用的RAII的风格，借助share_ptr，确认的client关闭时，手动将对应的引用计数变为0，触发下面的从两处删除

    //close()
    close(fd);
    cout << "close fd:" << fd << endl;
    //从已连接队列中删除
    clientFD_vector.remove(fd);
    //从epoll监控列表中删除
    delete_event(fd, EPOLLIN);
    //结束当前线程
    //pthread_exit(NULL);

}

void Ser::do_conf(const char *filename) {
    bzero(m_confpath, sizeof(m_confpath));
    FILE *pconf = fopen(filename, "rb");

    if (nullptr == pconf) {
        cout << "配置文件不存在" << endl;
        exit(-1);
    }

    char line[1024] = {0};
    while (fgets(line, sizeof(line), pconf) != nullptr) {
        char key[32] = {0};
        char path[1024] = {0};
        //sscanf(line, "%s=%s\n", key, path);
        char *p = line;
        while (*(p++) != '=');
        p[-1] = '\0';
        p[strlen(p) - 1] = '\0';//去掉换行符
        strcpy(key, line);
        strcpy(path, p);

        upchar(key, sizeof(key));
        //cout << key << path << endl;
        if (strcmp(key, "PATH") == 0) {
            strncpy(m_confpath, path, sizeof(path));
            if (m_confpath[strlen(m_confpath) - 1] == '/')//去掉后缀/
            {
                m_confpath[strlen(m_confpath) - 1] = '\0';
            }
            //cout << m_confpath << endl;
        }

    }
    return;
}

void Ser::start() {
    do_conf("./.httpd.conf");//从配置文件加载配置项
//	cout << "m_confpath:" << m_confpath << endl;


    cout << "line 271" << endl;

    while (true) {
        int num = wait_event();
        for (int i = 0; i < num; ++i) {

            int polled_fd = m_epoll_event[i].data.fd;

            unsigned eventType = m_epoll_event[i].events;

            //由于只创建了一个epoll对象（也就是只拥有一个epoll fd），所以这唯一的一个poll队列要用于的监听两种活动：
            //1、新的客户端连接：将新的客户端对应的fd加入到epoll的监控队列
            //2、客户端连接发来消息：调用相应的线程处理客户端的请求，最终就是想客户端对应的socket文件写入字节流

            //和读取kafka一样，还要自己再过滤一遍？？？？
            if ((polled_fd == serverFD) && (eventType & EPOLLIN)) {
                do_accept();
                //thread(&Ser::do_accept, this).detach();//启动分离的无名线程处理新连接
            } else if (eventType & EPOLLIN) {

                //处理并相应client发过来的字节流必然是在这个函数里了
                do_in(polled_fd);

                //thread(&Ser::do_in, this, fd).detach();//启动线程处理此请求事件
            }
            /*else if (event & EPOLLOUT)
                //thread out(do_out, fd);
                do_out(fd);*/
        }
        //由于连接关闭。当连接数大于100，且容器大于连接数的2倍时，适当减小事件处理容器
        size_t sizelist = clientFD_vector.size();
        size_t sizearr = m_epoll_event.size() / 2;
        if ((sizelist > 100) && (sizearr > sizelist))
            m_epoll_event.resize(sizearr);
    }
}


unsigned int Ser::readline(int fd, char *buf, size_t len) {
    unsigned int i{0};
    for (i = 0; i < len; ++i) {

        //linux中，文件都是是以文件描述符标记的，读取socket、设备的输入等，就是直接read对应的文件描述符
        if (read(fd, buf + i, 1) != 1) {
            if (errno == EINTR)
                continue;
            else return 0;
        }
        if (*(buf + i) == '\n' && *(buf + i - 1) == '\r')
            break;
    }
    return i + 1;
}

unsigned int Ser::writeline(int fd, const char *buf, size_t len) {
    unsigned int i = 0;
    for (i = 0; i < len; ++i) {
        if (write(fd, buf + i, 1) != 1) {
            if (errno == EINTR) {
                --i;
                continue;
            } else return 0;
        }
        if (*(buf + i) == '\n' && *(buf + i - 1) == '\r')
            break;
    }
    return i + 1;
}

unsigned int Ser::readn(int fd, char *buf, size_t len) {
    char *now = buf;
    size_t oready = 0;
    size_t ward = len;
    int ret = 0;
    while (ward) {
        do {//中断恢复
            ret = read(fd, now, ward);
        } while ((ret == -1) && (errno == EINTR));

        if (ret == -1)
            ERR_EXIT("readn");
        else if (ret == 0)//连接中断再也没有数据
            return 0;

        now += ret;
        oready += ret;
        ward -= ret;
    }
    return oready;
}

unsigned int Ser::writen(int fd, const char *buf, size_t len) {
    const char *now = buf;
    size_t oready = 0;
    size_t ward = len;
    int ret = 0;
    while (ward) {
        do {
            ret = write(fd, buf, ward);
        } while ((ret == -1) && (errno == EINTR));

        if (ret <= 0)
            ERR_EXIT("writen");

        now += ret;
        oready += ret;
        ward -= len;
    }
    return oready;
}

void Ser::upchar(char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i)
        *(buf + i) = toupper(*(buf + i));
}

void Ser::downchar(char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i)
        *(buf + i) = tolower(*(buf + i));
}

long Ser::get_file_size(const char *path) {
    long sizefile = -1;
    struct stat statbuf;
    if (stat(path, &statbuf) < 0)
        return sizefile;
    sizefile = statbuf.st_size;
    return sizefile;
}

void Ser::do_get(int fd, const char *uri, size_t len) {
    //解析uri指定的文件及参数
    //cout << "do_get" << endl;
    //静态文件则发送文件
    char filepath[1024] = {0};
    snprintf(filepath, sizeof(filepath), "%s%s", m_confpath, uri);
    if (filepath[strlen(filepath) - 1] == '/')
        strcat(filepath, "index.html");
    //cout << filepath << endl;
    if (!writeline(fd, "HTTP/1.1 200 OK\r\n", sizeof("HTTP/1.1 200 OK\r\n"))) {
        do_close(fd);
        return;
    }

    if (!writeline(fd, "Content-Type: text/html\r\n", sizeof("Content-Type: text/html\r\n"))) {
        do_close(fd);
        return;
    }
    long filesize = get_file_size(filepath);
    char contentlength_buf[128] = {0};
    sprintf(contentlength_buf, "content-length: %lu\r\n", filesize);
    if (writeline(fd, contentlength_buf, sizeof(contentlength_buf)) == 0) {
        do_close(fd);
        return;
    }
    if (writeline(fd, "\r\n", sizeof("\r\n")) == 0) {
        do_close(fd);
        return;
    }
    //cout << "begin send file:" << filesize << endl;
    if (filesize <= 0) return;
    FILE *pfile = fopen(filepath, "rb");
    char buf[4096] = {0};
    int ret = 0;
    while (1) {
        ret = fread(buf, 1, sizeof(buf), pfile);
        buf[ret] = 0;
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else do_close(fd);
        }
        if (ret == 0)
            break;
        if (!writen(fd, buf, ret)) {
            do_close(fd);
            return;
        }
    }
    //动态请求则执行程序并发送输出
}

void Ser::do_post(int fd, const char *uri, size_t urilen, const char *text, size_t textlen) {
    cout << "do_post" << endl;
    //解析uri
    //解析使用text报文体
    //发送相应内容
}
