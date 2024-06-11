#include "sql_connection_pool.h"


sql_connection_pool::sql_connection_pool() {
	m_cur_conn = 0;
	m_free_conn = 0;
}

sql_connection_pool* sql_connection_pool::GetInstance() {
	static sql_connection_pool connPool;
	return &connPool;
}

void sql_connection_pool::init(string url, string user, string password, string databasename,
	int port, int max_conn, int close_log) {
	m_url = url;
	m_user = user;
	m_password = password;
	m_databasename = databasename;
	m_port = port;
	m_close_log = close_log;

	for (int i = 0; i < max_conn; i++) {
		MYSQL* conn = NULL;

		//MYSQL对象初始化
		conn = mysql_init(conn);
		if (conn == NULL) {
			//LOG_ERROR("MySql Error");
			exit(1);
		}

		//连接数据库引擎
		conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(),
			databasename.c_str(), port, NULL, 0);

		if (conn == NULL) {
			//LOG_ERROR("Mysql Connect Error");
			exit(1);
		}
		
		//将连接的句柄(数据库连接)加到链表里
		connList.push_back(conn);

		//每加一个则多一个空闲数据库连接
		++m_free_conn;
	}

	reserve = sem(m_max_conn);

	m_max_conn = m_free_conn;
}

MYSQL* sql_connection_pool::GetConnection() {
	MYSQL* conn = NULL;

	if (0 == connList.size()) {
		return NULL;
	}

	reserve.wait();

	lock.lock();

	conn = connList.front();
	connList.pop_front();

	--m_free_conn;
	++m_cur_conn;

	lock.unlock();

	return conn;
}

bool sql_connection_pool::RealseConnection(MYSQL* conn) {
	if (NULL == conn) {
		return false;
	}

	lock.lock();

	connList.push_back(conn);

	++m_free_conn;
	--m_cur_conn;

	lock.unlock();

	reserve.post();

	return true;
}

int sql_connection_pool::GetFreeConn() {
	return this->m_free_conn;
}

void sql_connection_pool::DestroyPool() {
	lock.lock();
	if (connList.size() > 0) {
		list<MYSQL*>::iterator it;
		for (it = connList.begin(); it != connList.end(); it++) {
			MYSQL* con = *it;
			mysql_close(con);
		}

		m_cur_conn = 0;
		m_free_conn = 0;
		connList.clear();
	}
	lock.unlock();

}

sql_connection_pool::~sql_connection_pool() {
	DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL,sql_connection_pool * connPool) {
	*SQL = connPool->GetConnection();
	connRAII = *SQL;
	connRAII = connRAII;
}

connectionRAII::~connectionRAII() {
	pollRAII->RealseConnection(connRAII);
}