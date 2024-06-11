#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include "lock.h"

using namespace std;

class sql_connection_pool {

public:
	MYSQL* GetConnection();					//获取数据库的连接
	bool RealseConnection(MYSQL* conn);		//释放当前使用的连接
	int GetFreeConn();						//获取空闲连接数量
	void DestroyPool();						//销毁所有链接

	//单例模式
	static sql_connection_pool* GetInstance();

	void init(string url, string user, string password, string databasename,
				int port, int max_conn,int close_log);

private:
	sql_connection_pool();
	~sql_connection_pool();
	int m_max_conn;		//最大连接数
	int m_cur_conn;		//当前已连接
	int m_free_conn;	//当前空闲
	locker lock;

	list<MYSQL*>connList;	//连接池
	sem reserve;


public:
	int m_close_log;
	string m_port;
	string m_user;
	string m_password;
	string m_databasename;
	string m_url;

};

class connectionRAII {

public:
	connectionRAII(MYSQL **SQL, sql_connection_pool * connPool);
	~connectionRAII();


private:
	MYSQL *connRAII;
	sql_connection_pool* pollRAII;
};
#endif