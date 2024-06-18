#ifndef LOG_H
#define LOG_h

#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;


class Log {
public:

	//����ģʽ-����ʽ
	static Log* get_instance() {
		static Log instance;
		return &instance;
	}

	static void* flush_log_thread(void* args) {
		Log::get_instance()->async_write_log();
	}

	//��ѡ��Ĳ�������־�ļ�����־��������С����������Լ����־������
	bool init(const char* fime, int close_log, int long_buf_size = 8192, 
		int split_lines = 5000000, int max_queue_size = 0);

	void write_log(int level, const char *format, ...);

	void flush(void);

private:
	Log();
	virtual ~Log();

	void* async_write_log() {
		string single_log;
		//������������ȡ��һ����־string��д���ļ�
		while (m_log_queue->pop(single_log)) {
			m_mutex.lock();
			fputs(single_log.c_str(), m_fp);
		}
	}

private:
	block_queue<string> *m_log_queue;	//��������
	locker m_mutex;
	FILE* m_fp;					//��log���ļ�ָ��
	int m_close_log;			//�Ƿ�����־
	int m_log_buf_size;			//��־��������С
	int m_split_lines;			//��־�������
	bool m_is_async;			//�Ƿ����첽
	long long m_count;			//��־������¼
	char* m_buf;				//Ҫ���������
	char log_name[128];			//log�ļ���
	char dir_name[128];			//·����
	int m_today;				//������࣬��¼��ǰ����һ��
};

#define LOG_DEBUG(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(0,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(1,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(2,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(3,format,##__VA_ARGS__); Log::get_instance()->flush();}

#endif