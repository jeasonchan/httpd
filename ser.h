#include "common.h"
//TODO
//将IO函数提出到单独模块
//将网络部分提出，内部使用线程，封装成异步IO接口,写也改为非堵塞
//将业务部分独立
//考虑跨平台

class Ser {
		//friend void do_accept(Ser& ser);//有新连接进入时调用
		//friend void do_in(Ser& ser, int fd);//有可读事件发生时
	public:
		Ser();
		Ser(const char* ip, unsigned int port);

		//析构啥也不干？？？？这个点类让别人使用时，可能就会造成文件描述符绑定的资源得不到释放，资源泄露
		~Ser() {}
	public:
		void start();
		int wait_event();//监听一次所有套接口
		void do_accept();//有新连接进入时调用
		void do_in(int fd);//有可读事件发生时
		void do_close(int fd);//断开链接
	public:
		/*void do_out(int fd);//有可写事件发生时*/

	private://报文处理函数
		unsigned int readline(int fd, char* buf, size_t len); //读取一行,返回值为读取字符个数，链接断开则返回0
		unsigned int writeline(int fd, const char* buf, size_t len);//写一行,写到\r\n结束，返回值不是正数则出错
		unsigned int readn(int fd, char* buf, size_t len);//读取len个字符,0则表示断开链接
		unsigned int writen(int fd, const char* buf, size_t len);//写入len个字符,返回值不为正数则出错
		void upchar(char* buf, size_t len);
		void downchar(char* buf, size_t len);
		void do_get(int fd, const char* uri, size_t len);
		void do_post(int fd, const char* uri, size_t urilen, const char* text, size_t textlen);
		long get_file_size(const char* path);
	private://私用方法
		void do_conf(const char*filename);
		void Bind(int sockfd, struct sockaddr_in* addr);//初始化工作
		void Listen(int sockfd, unsigned int num);//初始化工作
		void add_event(int fd, int state);//添加一个事件到epoll
		void delete_event(int fd, int state);
		void modify_event(int fd, int state);
	private://数据

        //server本身的套接字
		int serverFD;

		struct sockaddr_in m_local_addr;//监听套接字的地址结构
		int m_epoll_fd;
		socklen_t m_addr_len;//地址长度
	
		char m_confpath[1024];//默认路径配置
		
		list<int> clientFD_vector;//已连接套接字队列
		vector<struct epoll_event> m_epoll_event;//待处理事件队列
};

