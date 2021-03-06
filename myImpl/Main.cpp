#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <string>

//使用CPP转接过来的C的头，而不是用原始的.h的头文件  更加纯粹的CPP
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <functional>
#include "Util.hpp"
#include "Socket.hpp"

namespace jeason {
    const int MAX_EVENTS = 500;
    const int MAX_CHAR_LENGTH = 10000;
}

using namespace jeason;

int main() {
    //定义并全初始化为0，
    char streamBuffer[jeason::MAX_EVENTS][jeason::MAX_CHAR_LENGTH]{0};
    int nCount = 0;

    //获取socket文件描述符
    int serverSocketFD = -1;
    serverSocketFD = socket(PF_INET, SOCK_STREAM, 0);

    if (-1 == serverSocketFD) {
        jeason::Util::exit_with_flag_errInfo("socket fail");
    }
    std::cout << "listenSocketFD:" << serverSocketFD << std::endl;


    //在协议层面，修改的socket的属性
    int bReuse = 1;
    if (-1 == setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, (char *) &bReuse, sizeof(bReuse))) {
        Util::exit_with_flag_errInfo("setSockOpt fail");
    }

    //绑定监听的端口
    sockaddr_in serverAddress{AF_INET, htons(8888), {INADDR_ANY}};

    /*
    以下是C风格的结构体赋值方法，很显然，使用花括号初始化是直接初始化，省去了先初始化再赋值的开销

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8888);//本质就是short int
    */

    if (-1 == bind(serverSocketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress))) {
        if (-1 == bind(serverSocketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress))) {
            jeason::Util::exit_with_flag_errInfo("bind fail");
        }

        //等待client端接入，开始监听
        if (-1 == listen(serverSocketFD, 3)) {
            jeason::Util::exit_with_flag_errInfo("listen fail");
        }

        //创建epoll队列，队列大小为500,文件描述符其实就像指针，获取之后要及时关闭,RAII可以通过转移构造和share_ptr实现
        int epollFD = -1;
        epollFD = epoll_create(jeason::MAX_EVENTS);
        if (-1 == epollFD) {
            jeason::Util::exit_with_flag_errInfo("epoll_create fail");
        }
        std::cout << "epollFD:" << epollFD << std::endl;

        //设置该epoll队列所关注的事件和文件描述符
        epoll_event epollEventConfig{EPOLLIN, {}}, epollEventsQueue[jeason::MAX_EVENTS];

        /*
        //设置队列的所关注的事件类型是EPOLLIN
        epollEventConfig.events =EPOLLIN;
        //设置队列的所关注的事件是发生在该FD上的事件
        epollEventConfig.data.fd = serverSocketFD;
        */

        //
        if (-1 == epoll_ctl(epollFD, EPOLL_CTL_ADD, serverSocketFD, &epollEventConfig)) {
            std::cerr << "epoll_create fail\n" << std::endl;
            return -1;
        }


        while (true) {
            int i = 0;
            struct sockaddr_in peeraddr{};
            int addrlen = sizeof(peeraddr);
            int fd;
            int rcvnum = 0;
            int r = 9;

            memset(&peeraddr, 0, sizeof(peeraddr));

            nCount = epoll_wait(epollFD, epollEventsQueue, jeason::MAX_EVENTS, -1);
            if (0 > nCount) {
                std::cerr << "epoll_create fail\n" << std::endl;
                continue;
            }

            //对得到的nCount个事件进行遍历
            for (i = 0; i < nCount; i++) {
                if (epollEventsQueue[i].data.fd == serverSocketFD) {
                    fd = accept(serverSocketFD, (struct sockaddr *) &peeraddr, reinterpret_cast<socklen_t *>(&addrlen));
                    printf("peer addr %s:%d\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));


                    epollEventConfig.events = EPOLLIN;
                    epollEventConfig.data.fd = fd;
                    if (-1 == epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &epollEventConfig)) {
                        perror("epoll_ctl listen fail\n");
                        return -1;
                    }
                } else {
                    rcvnum = recv(epollEventsQueue[i].data.fd, streamBuffer[i], 100, 0);
                    if (0 == rcvnum) {
                        close(epollEventsQueue[i].data.fd);
                        continue;
                    }
                    streamBuffer[i][99] = 0;
                    std::cout << "recv:streamBuffer\n" << streamBuffer[i] << std::endl;
                    memset(streamBuffer[i], 0, 100);
                }
            }
        }

        return 0;
    }
}