#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <map>
#include <errno.h>
#include <sys/mman.h>

#include "lock.h"
#include "sql_connection_pool.h"
#include "lst_timer.h"
#include "log.h"

class http_conn {
public:
	http_conn() {}
	~http_conn(){}

public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;

	enum METHOD {	GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH};

	enum CHECK_STATE {
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEAD,
		CHECK_STATE_CONTENT
	};

	enum HTTP_CODE{
		NO_REQUEST=0,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};

	enum LINE_STATUS {
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN
	};

public:
	void init(int sockfd, const sockaddr_in& address, char* root, 
		int TRIGMode, int close_log, string user, string passwd, string sqlname);
	void close_conn(bool real_close = true);
	void initmysql_result(sql_connection_pool *connPool);
	bool read_once();
	bool write();
	void process();
	sockaddr_in* get_address() { return &m_address; };

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);
	LINE_STATUS parse_line();
	HTTP_CODE parse_request_line(char *text);
	HTTP_CODE parse_headers(char *text);
	HTTP_CODE parse_content(char *text);
	HTTP_CODE do_request();
	char* get_line() { return m_read_buf + m_start_line; };

	bool add_content(const char* content);
	bool add_status_line(int status, const char* title);
	bool add_headers(int content_length);
	bool add_response(const char *format, ...);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();
	void unmap();



public:
	static int m_epollfd;
	static int m_user_count;
	MYSQL* mysql;
	int m_state;

	int timer_flag;
	int improv;

private:
	int m_sockfd;
	sockaddr_in m_address;
	int m_TRIGMode;
	int m_close_log;
	char* doc_root;
	int bytes_to_send;
	int bytes_have_send;
	CHECK_STATE m_check_state;
	METHOD m_method;
	char* m_url;
	char* m_version;
	char* m_host;
	long m_content_length;
	bool m_linger;
	int cgi;		//是否启用的POST，个人理解为是否需要跟数据库进行对比
	int m_read_idx;
	int m_write_idx;
	int m_start_line;
	char m_read_buf[READ_BUFFER_SIZE];
	char m_write_buf[WRITE_BUFFER_SIZE];
	char m_real_file[FILENAME_LEN];
	int m_checked_idx;
	char* m_string;
	struct stat m_file_stat;
	struct iovec m_iv[2];
	int m_iv_count;
	char* m_file_address;

	char sql_user[200];
	char sql_passwd[200];
	char sql_name[200];
	
};


#endif