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

	//单例模式-懒汉式
	static Log* get_instance() {
		static Log instance;
		return &instance;
	}

	static void* flush_log_thread(void* args) {
		Log::get_instance()->async_write_log();
	}

	//可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
	bool init(const char* fime, int close_log, int long_buf_size = 8192, 
		int split_lines = 5000000, int max_queue_size = 0);

	void write_log(int level, const char *format, ...);

	void flush(void);

private:
	Log();
	virtual ~Log();

	void* async_write_log() {
		string single_log;
		//从阻塞队列中取出一个日志string，写入文件
		while (m_log_queue->pop(single_log)) {
			m_mutex.lock();
			fputs(single_log.c_str(), m_fp);
		}
	}

private:
	block_queue<string> *m_log_queue;	//阻塞队列
	locker m_mutex;
	FILE* m_fp;					//打开log的文件指针
	int m_close_log;			//是否开启日志
	int m_log_buf_size;			//日志缓冲区大小
	int m_split_lines;			//日志最大行数
	bool m_is_async;			//是否开启异步
	long long m_count;			//日志行数记录
	char* m_buf;				//要输出的内容
	char log_name[128];			//log文件名
	char dir_name[128];			//路径名
	int m_today;				//按天分类，记录当前是哪一天
};

#define LOG_DEBUG(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(0,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(1,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(2,format,##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format,...) if(0 == m_close_log) {Log::get_instance()->write_log(3,format,##__VA_ARGS__); Log::get_instance()->flush();}

#endif