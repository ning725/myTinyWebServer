#include "config.h"


Config::Config() {

	//�˿ں�Ĭ��Ϊ10000
	PORT = 10000;

	//��־д�뷽ʽ��Ĭ��ͬ��
	LOGWrite = 0;

	//�������ģʽ��Ĭ��listenfd LT + connfd LT
	TRIGMode = 0;

	//listenfd����ģʽ��Ĭ��LT
	LISTENTrigmode = 0;

	//connfd����ģʽ��Ĭ��LT
	connfdTrigmode = 0;

	//���Źر����ӣ�Ĭ�ϲ�ʹ��
	OPT_LINGER = 0;

	//���ݿ����ӳ�����,Ĭ��8
	sql_num = 8;

	//�̳߳��ڵ��߳�����,Ĭ��8
	thread_num = 8;

	//�ر���־,Ĭ�ϲ��ر�
	close_log = 0;

	//����ģ��,Ĭ����proactor
	actor_model = 0;
}


//�û�����ͨ�������в��������ǳ�ʼֵ
void Config::parse_arg(int argc, char *argv[]) {
	int opt;
	const char* str = "p:l:m:o:s:t:c:a:";
	while ((opt = getopt(argc, argv, str) != -1)) {
		switch (opt) {
			case 'p': {
				PORT = atoi(optarg);
				break;
			}
			case 'l': {
				
				break;
			}
			case 'm': {

				break;
			}
			case 'o': {

				break;
			}
			case 's': {

				break;
			}
			case 't': {

				break;
			}
			case 'c': {

				break;
			}
			case 'a': {

				break;
			}
			default:
				break;
		}
	}


}