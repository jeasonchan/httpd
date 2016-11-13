#include "ser.h"

Ser::Ser()
{
	if ((m_listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	  ERR_EXIT("socket");
	
	addr_len = sizeof(my_addr);
	bzero(&my_addr, addr_len);
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(80);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	Bind(m_listenfd, &my_addr);
	Listen(m_listenfd, LISTENQ);
	
	if ((epoll_fd = epoll_create1(0)) < 0)
	  ERR_EXIT("epoll_create1");
	add_event(m_listenfd, EPOLLIN);
	
	struct epoll_event one;
	m_epoll_event.push_back(one);
}

Ser::Ser(const char* ip, unsigned int port)
{
	if ((m_listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	  ERR_EXIT("socket");
	
	addr_len = sizeof(my_addr);
	bzero(&my_addr, addr_len);
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &my_addr) < 0)
	  ERR_EXIT("inet_pton");

	Bind(m_listenfd, &my_addr);
	Listen(m_listenfd, LISTENQ);

	if ((epoll_fd = epoll_create1(0)) < 0)
	  ERR_EXIT("epoll_create1");
	add_event(m_listenfd, EPOLLIN);
	
	struct epoll_event one;
	bzero(&one, sizeof(one));
	m_epoll_event.push_back(one);
}

void Ser::Bind(int sockfd, struct sockaddr_in* addr)
{
	if (bind(sockfd, (SA*)addr, addr_len) < 0)
		ERR_EXIT("bind");
}

void Ser::Listen(int sockfd, unsigned int num)
{
	if (listen(sockfd, num) < 0)
	  ERR_EXIT("listen");
}

int  Ser::wait_event()
{
	int ret = epoll_wait(epoll_fd, &*m_epoll_event.begin(), m_epoll_event.size()+1, -1);
	if (ret < 0)
	{
		ERR_EXIT("epoll_wait");;
	}
	return ret;
}

void Ser::add_event(int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);	
}
void Ser::delete_event(int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}
void Ser::modify_event(int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void Ser::do_accept()
{
	int fd = accept(m_listenfd, nullptr, nullptr);
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
	
	if (fd == -1)
	{
		cerr << "accept" << endl;
		return;
	}
	cout << "new fd:" << fd << endl;

	m_connfd.push_back(fd);//添加到已连接队列
	add_event(fd, EPOLLIN);//添加到epoll监听

	struct epoll_event ev;
	bzero(&ev, sizeof(ev));
	m_epoll_event.push_back(ev);//扩大事件处理队列容量
}

void Ser::do_in(int fd)
{
	char head[HEADSIZE];
	//读报文头
	if (readline(fd, head, HEADSIZE) == 0)
		do_close(fd);
	//read返回0则链接断开,处理已链接列表，处理epoll监听队列,减小数组大小
	//解析存储
	char way[16] = {0};
	char uri[1024] = {0};
	char version[16] = {0};
	sscanf(head, "%s %s %s\r\n", way, uri, version);
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
			if (ret == 0)
			  do_close(fd);
			if ((ret == 2) && (bodybuf[0] == '\r') && (bodybuf[1] == '\n'))
			  break;
			bzero(bodybuf, BODYSIZE);
		}
		do_get(fd, uri, sizeof(uri));//解析数据并完成发送任务
	}
	else if (strcmp(way, "POST") == 0)//post方法请求
	{
		char bodybuf[BODYSIZE] = {0};
		char keybuf[32] = {0};
		char valuebuf[512] = {0};
		size_t textlen = 0;
		int ret;
		while (1)//直到请求头结束
		{
			ret = readline(fd, bodybuf, BODYSIZE);
			if (ret == 0)
			  do_close(fd);
			if ((ret == 2) && (bodybuf[0] == '\r') && (bodybuf[1] == '\n'))
			  break;

			downchar(bodybuf, BODYSIZE);//统一转为小写
			bzero(keybuf, sizeof(keybuf));
			bzero(valuebuf, sizeof(valuebuf));
			sscanf(bodybuf, "%s: %s\r\n", keybuf, valuebuf);//分解键值对
			if (strcmp(keybuf, "content-length") == 0)
			{
				sscanf(valuebuf, "%lu", &textlen);//读取文本长度
			}
			bzero(bodybuf, BODYSIZE);
		}

		char *ptext = new char[textlen+1];
		bzero(ptext, textlen+1);
		if (readn(fd, ptext, textlen) == 0)//读取文本
		{
			delete[] ptext;
			do_close(fd);
		}
		ptext[textlen] = '\0';
		do_post(fd, uri, sizeof(uri), ptext, textlen+1);//解析数据并完成发送任务
		delete[] ptext;
	}
	
	return;
}

void Ser::do_close(int fd)
{
	//close()
	close(fd);
	//从已连接队列中删除
	//从epoll中删除
	//适当减小事件处理容器
	//结束当前线程
}

void Ser::do_conf(const char* filename)
{
	bzero(confpath, sizeof(confpath));
	FILE* pconf = fopen(filename, "rb");
	char line[1024] = {0};
	while (fgets(line, sizeof(line), pconf) != NULL)
	{
		char key[32] = {0};
		char path[1024] = {0};
		//sscanf(line, "%s=%s\n", key, path);
		char* p = line;
		while (*(p++) != '=');
		p[-1] = '\0';
		strcpy(key, line);
		strcpy(path, p);

		upchar(key, sizeof(key));
		//cout << key << path << endl;
		if (strcmp(key, "PATH") == 0)
		{
			strncpy(confpath, path, sizeof(path));
			if (confpath[strlen(confpath)-1] == '/')//去掉后缀/
			{
				confpath[strlen(confpath)-1] = '\0';
			}
		}
		
	}
	return;
}

void Ser::go()
{
	do_conf("./.httpd.conf");//从配置文� 加载配置项
	cout << "confpath:" << confpath << endl;
	while (1)
	{
		int num = wait_event();
		for (int i = 0; i < num; ++i)
		{
			int fd = m_epoll_event[i].data.fd;
			int event = m_epoll_event[i].events;
			
			if ((fd == m_listenfd) && (event & EPOLLIN))
			{
				thread(&Ser::do_accept, *this).detach();//启动分离的无名线程处理新连接
			}
			else if (event & EPOLLIN)
			{
				thread(&Ser::do_in, *this, fd).detach();//启动线程处理此请求事件
			}
			/*else if (event & EPOLLOUT)
				//thread out(do_out, fd);
				do_out(fd);*/
		}

	}
}

unsigned int Ser::readline(int fd, char* buf, size_t len)
{

	return 0;
}
unsigned int Ser::writeline(int fd, const char* buf, size_t len)
{
	return 0;
}
unsigned int Ser::readn(int fd, char* buf, size_t len)
{
	char* now = buf;
	size_t oready = 0;
	size_t ward = len;
	int ret = 0;
	while (ward)
	{
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
unsigned int Ser::writen(int fd, const char* buf, size_t len)
{
	const char* now = buf;
	size_t oready = 0;
	size_t ward = len;
	int ret = 0;
	while (ward)
	{
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
void Ser::upchar(char* buf, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	  *(buf+i) = toupper(*(buf+i));
}
void Ser::downchar(char* buf, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	  *(buf+i) = tolower(*(buf+i));
}
void Ser::do_get(int fd, const char* uri, size_t len)
{
	//解析uri指定的文件及参数
	//静态文件则发送文件
	//动态请求则执行程序并发送输出
}
void Ser::do_post(int fd, const char* uri, size_t urilen, const char* text, size_t textlen)
{
	//解析uri
	//解析使用text报文体
	//发送相应内容
}
