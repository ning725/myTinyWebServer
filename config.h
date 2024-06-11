#ifndef CONFIG_H
#define CONFIG_H
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "webserver.h"

using namespace std;

class Config {
public:
	Config();
	~Config() {};

	void parse_arg(int argc, char* argv[]);

	//�˿ں�
	int PORT;

	//��־д�뷽ʽ
	int LOGWrite;

	//����ģʽ
	int TRIGMode;

	//listen����ģʽ
	int LISTENTrigmode;

	//connfd����ģʽ
	int connfdTrigmode;

	//���Źر�����
	int OPT_LINGER;

	//���ݿ����ӳ�����
	int sql_num;

	//�̳߳��ڵ��߳�����
	int thread_num;

	//�Ƿ�ر���־
	int close_log;

	//����ģ��ѡ��
	int actor_model;

};


#endif