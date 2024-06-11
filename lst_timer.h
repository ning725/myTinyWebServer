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

	//���ļ����������÷�����
	int setnonblocking(int fd);

	//���ں�ʱ��ע����¼���ETģʽ��ѡ����EPOLLONESHOT
	void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

	//�źŴ�����
	static void sig_handler(int sig);

	//�����źź���
	void addsig(int sig, void(handler)(int), bool restart = true);

	//��ʱ�����������¶�ʱ�Բ��ϳ���SIGALRM�ź�
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