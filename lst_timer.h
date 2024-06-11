#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

class util_timer;

struct client_data {

	sockaddr_in address;
	int sockfd;
	util_timer* timer;

};

class util_timer {
public:
	util_timer(): prev(NULL), next(NULL) {}

public:
	time_t expire;

	void(*cb_func)(client_data*);
	client_data* user_data;
	util_timer* prev;
	util_timer* next;
};

class sort_timer_list {
public:
	sort_timer_list();
	~sort_timer_list();

	void add_timer(util_timer* timer);
	void adjust_timer(util_timer* timer);
	void del_timer(util_timer* timer);
	void tick();

private:
	void add_timer(util_timer* timer, util_timer* lst_head);

	util_timer* head;
	util_timer* tail;


};

class Utils {
public:
	Utils() {}
	~Utils() {}

	void init(int timeslot);

	//对文件描述符设置非阻塞
	int setnonblocking(int fd);

	//将内核时间注册读事件，ET模式，选择开启EPOLLONESHOT
	void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

	//信号处理函数
	static void sig_handler(int sig);

	//设置信号函数
	void addsig(int sig, void(handler)(int), bool restart = true);

	//定时处理任务，重新定时以不断出发SIGALRM信号
	void timer_handler();

	void show_error(int connfd, const char* info);

public:
	static int *m_pipefd;
	sort_timer_list m_timer_list;
	static int m_epollfd;
	int m_TIMESLOT;

};

void cb_func(client_data*user_data);

#endif