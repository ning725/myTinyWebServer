#include "lst_timer.h"

sort_timer_list::sort_timer_list(){
	head = NULL;
	tail = NULL;
}

sort_timer_list::~sort_timer_list() {
	util_timer* temp = head;
	while (temp) {
		head = temp->next;
		delete temp;
		temp = head;
	}
}

void sort_timer_list::add_timer(util_timer* timer) {
	if (!timer) {
		return;
	}

	if (!head) {
		head = tail = timer;
		return;
	}

	//如果当前时间比头指针时间小则插入
	if (timer->expire < head->expire) {
		timer->next = head;
		head->prev = timer;
		head = timer;
	}

	//比头指针大则进入以下函数，进行查找插入
	add_timer(timer,head);
}

void sort_timer_list::adjust_timer(util_timer *timer) {
	if (!timer) {
		return;
	}
	
	util_timer* tmp = timer->next;

	//如果被调整的目标处于链表尾部，或者 他的下一个定时器的值仍然比他大，直接返回
	if (!tmp || (timer->expire < tmp->expire)) {
		return;
	}

	//如果被调整的目标处于链表的定时器则将他取出，重新插入
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		timer->next = NULL;
		add_timer(timer, head);
	}

	//不是头节点也不是尾节点，将定时器取出，插入在他原来位置之后的位置
	//因为维护的是升序链表
	else {
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);
	}
}

void sort_timer_list::del_timer(util_timer* timer) {
	if (!timer) {
		return;
	}

	//如果是头节点并且是尾节点
	if ((timer == head) && (timer == tail)) {
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}

	//头节点
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}

	//尾节点
	if (timer == tail) {
		tail = tail->prev;
		tail->next = NULL;
		delete timer;
		return;
	}

	//以上都不是
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

void sort_timer_list::tick() {
	if (!head) {
		return;
	}

	time_t cur = time(NULL);	//获得系统当前时间
	util_timer* temp = head;

	while (temp) {
		if (cur < temp->expire) {
			break;
		}

		temp->cb_func(temp->user_data);
		head = temp->next;

		if (head) {
			head->prev = NULL;
		}
		delete temp;
		temp = head;
	}
}

void sort_timer_list::add_timer(util_timer* timer, util_timer* lst_head) {
	util_timer* prev = lst_head;
	util_timer* temp = lst_head->next;

	while (temp) {
		if (timer->expire < temp->expire) {
			prev->next = timer;
			timer->next = temp;
			temp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = temp;
		temp = temp->next;
	}

	//如果遍历完了还没有找到则设置韦新的尾节点
	if (!temp) {
		prev->next = timer;
		timer->prev = prev;
		timer->next = NULL;
		tail = timer;
	}
}

void Utils::init(int timeslot) {
	m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd) {
	int old_flag = fcntl(fd, F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, new_flag);

	return old_flag;
}

//将内核时间表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
	epoll_event event;

	//listenfd LT + connfd ET
	if (1 == TRIGMode) {
		event.events = EPOLLIN | EPOLLET | EPOLLHUP;
	}
	else
		event.events = EPOLLIN | EPOLLHUP;

	if (one_shot) {
		event.events |= EPOLLONESHOT;
	}

	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig) {
	
	//确保函数的可重入性，保留原来的errno
	int save_errno = errno;
	int msg = sig;
	send(m_pipefd[1], (char*)&msg,1,0);

	errno = save_errno;

}

void Utils::addsig(int sig, void(handler)(int), bool restart) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}

	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);

}

//定时处理任务，重新定时以不断出发SIGALRM信号
void Utils::timer_handler() {
	m_timer_list.tick();
	alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info) {
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

int* Utils::m_pipefd = 0;
int Utils::m_epollfd = 0;

void cb_func(client_data* user_data) {
	epoll_ctl(Utils::m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);
	
	//user - 1
}