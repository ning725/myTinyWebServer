#include "config.h"

int main(int argc, char* argv[]) {
	
	//���ݿ���Ϣ
	string user = "root";
	string passwd = "123321";
	string databasename = "xcdb";

	//�����н���
	Config config;
	config.parse_arg(argc,argv);

	WebServer server;

	//��ʼ��
	server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
		config.OPT_LINGER, config.TRIGMode, config.sql_num, config.thread_num,
		config.close_log, config.actor_model);

	//��־
	server.log_write();

	//���ݿ�
	server.sql_pool();

	//�̳߳�
	server.thread_pool();

	//����ģʽ
	server.trig_mode();

	//�����¼�
	server.eventListen();

	//����
	//server.eventLoop();
}
