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
	MYSQL* GetConnection();					//��ȡ���ݿ������
	bool RealseConnection(MYSQL* conn);		//�ͷŵ�ǰʹ�õ�����
	int GetFreeConn();						//��ȡ������������
	void DestroyPool();						//������������

	//����ģʽ
	static sql_connection_pool* GetInstance();

	void init(string url, string user, string password, string databasename,
				int port, int max_conn,int close_log);

private:
	sql_connection_pool();
	~sql_connection_pool();
	int m_max_conn;		//���������
	int m_cur_conn;		//��ǰ������
	int m_free_conn;	//��ǰ����
	locker lock;

	list<MYSQL*>connList;	//���ӳ�
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