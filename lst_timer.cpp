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

	//�����ǰʱ���ͷָ��ʱ��С�����
	if (timer->expire < head->expire) {
		timer->next = head;
		head->prev = timer;
		head = timer;
	}

	//��ͷָ�����������º��������в��Ҳ���
	add_timer(timer,head);
}

void sort_timer_list::adjust_timer(util_timer *timer) {
	if (!timer) {
		return;
	}
	
	util_timer* tmp = timer->next;

	//�����������Ŀ�괦������β�������� ������һ����ʱ����ֵ��Ȼ������ֱ�ӷ���
	if (!tmp || (timer->expire < tmp->expire)) {
		return;
	}

	//�����������Ŀ�괦������Ķ�ʱ������ȡ�������²���
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		timer->next = NULL;
		add_timer(timer, head);
	}

	//����ͷ�ڵ�Ҳ����β�ڵ㣬����ʱ��ȡ������������ԭ��λ��֮���λ��
	//��Ϊά��������������
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

	//�����ͷ�ڵ㲢����β�ڵ�
	if ((timer == head) && (timer == tail)) {
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}

	//ͷ�ڵ�
	if (timer == head) {
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}

	//β�ڵ�
	if (timer == tail) {
		tail = tail->prev;
		tail->next = NULL;
		delete timer;
		return;
	}

	//���϶�����
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

void sort_timer_list::tick() {
	if (!head) {
		return;
	}

	time_t cur = time(NULL);	//���ϵͳ��ǰʱ��
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

	//����������˻�û���ҵ�������Τ�µ�β�ڵ�
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

//���ļ����������÷�����
int Utils::setnonblocking(int fd) {
	int old_flag = fcntl(fd, F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, new_flag);

	return old_flag;
}

//���ں�ʱ���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
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

//�źŴ�����
void Utils::sig_handler(int sig) {
	
	//ȷ�������Ŀ������ԣ�����ԭ����errno
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

//��ʱ�����������¶�ʱ�Բ��ϳ���SIGALRM�ź�
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