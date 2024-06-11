#include "webserver.h"

WebServer::WebServer() {
	//http_conn类对象
	users = new http_conn[MAX_FD];

	//root文件夹路径
	char server_path[200];
	getcwd(server_path, 200);
	char root[6] = "/root";
	m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
	strcpy(m_root, server_path);
	strcat(m_root, root);

	//定时器
	


}

WebServer::~WebServer() {
	delete[] users;

}

void WebServer::init(int port, string user, string passwd, string databasename,
	int log_write, int opt_linger, int trigmode, int sql_num,
	int thread_num, int close_log, int actor_model) {

	m_port = port;
	m_user = user;
	m_passwd = passwd;
	m_databasename = databasename;
	m_log_write = log_write;
	m_opt_linger = opt_linger;
	m_TRIGmode = trigmode;
	m_sql_num = sql_num;
	m_thread_num = thread_num;
	m_close_log = close_log;
	m_actor_model = actor_model;

}

void WebServer::log_write() {
	if (0 == m_close_log) {
		
	}

}

void WebServer::sql_pool() {
	//初始化数据库连接池
	m_connPool = sql_connection_pool::GetInstance();
	m_connPool->init("localhost",m_user,m_passwd,m_databasename,3306,m_sql_num,m_close_log);

	//初始化
	//users.
}

void WebServer::thread_pool() {
	m_pool = new threadpool<http_conn>(m_actor_model, m_connPool, m_thread_num);
}

void WebServer::trig_mode() {
	//LT + LT
	if (0 == m_TRIGmode) {
		m_listenTrigmode = 0;
		m_connTrigmode = 0;
	}

	//LT + ET
	if (1 == m_TRIGmode) {
		m_listenTrigmode = 0;
		m_connTrigmode = 1;
	}

	//ET + LT
	if (2 == m_TRIGmode) {
		m_listenTrigmode = 1;
		m_connTrigmode = 0;
	}

	//ET + ET
	if (3 == m_TRIGmode) {
		m_listenTrigmode = 1;
		m_connTrigmode = 1;
	}
}

void WebServer::eventListen(){
	//网络编程基本步骤
	m_listenfd = socket(PF_INET, SOCK_STREAM, 0);

	//优雅关闭
	if (0 == m_opt_linger) {
		struct linger tmp = {0,1};
		setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
	}

	else if (1 == m_opt_linger) {
		struct linger tmp = { 1, 1 };
		setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
	}

	int ret = 0;

	struct sockaddr_in* address;
	bzero(address, sizeof(address));
	address->sin_family = PF_INET;
	address->sin_addr.s_addr = htonl(INADDR_ANY);
	address->sin_port = htons(m_port);

	int flag = 1;
	setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret >= 0);

	ret = listen(m_listenfd, 5);
	assert(ret >= 0);


	//epoll创建内核时间表
	epoll_event events[MAX_EVENT_NUMBER];
	m_epollfd = epoll_create(5);
	assert(m_epollfd != -1);


}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {


}