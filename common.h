#ifndef _COMMON_H_
#define _COMMON_H_
//linux
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

//C
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

//C++
#include <iostream>
#include <list>
#include <vector>
#include <string>
#include <thread>
//#include <utility>
//#include <chrono>
//#include <functional>
//#include <atomic>
using namespace std;

//常用常量
#define LOCAL_IP "119.29.4.18"
#define LOCALHOST_NAME "www.cppserver.cn"
#define SER_PORT 80
#define LISTENQ 1024

#define HEADSIZE 1024
#define BODYSIZE 1024

/**
 * 宏函数
 *
 * 打印字自定义的关键字，并打印当前的错误码
 *
 * @param str 用户方便自己定位的关键字
 */
#define ERR_EXIT(str) do {\
        cerr << str << " errno:" << errno << endl<<"err info:"<< strerror(errno) ;\
        exit(-1);\
} while (0)
//重命名结构体
typedef struct sockaddr SA;


#endif //_COMMON_H_
