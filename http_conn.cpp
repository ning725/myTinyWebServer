#include "http_conn.h"
#include <mysql/mysql.h>


//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_from = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string>users;

int setnonblocking(int fd){
	int old_flag = fcntl(fd, F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flag);
	return old_flag;
}

//将内核事件注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd,int fd, bool one_shot, int TRIGMode) {
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode) {
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	}
	else {
		event.events = EPOLLIN | EPOLLRDHUP;
	}

	if (one_shot) {
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}

//初始化链接，外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in& address, 
		  char* root, int TRIGMode, int close_log, 
		  string user, string passwd, string sqlname) 
{
	m_sockfd = sockfd;
	m_address = address;

	addfd(m_epollfd, sockfd, true, m_TRIGMode);
	m_user_count++;
	

	//当浏览器出现连接重置时
	//可能是网站根目录出错或http响应格式出错或文件中内容完全为空
	doc_root = root;
	m_TRIGMode = TRIGMode;
	m_close_log = close_log;
	
	strcpy(sql_user, user.c_str());
	strcpy(sql_passwd, passwd.c_str());
	strcpy(sql_name, sqlname.c_str());

	init();
}


//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init() {
	mysql = NULL;
	bytes_to_send = 0;
	bytes_have_send = 0;
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;
	m_write_idx = 0;
	cgi = 0;
	m_state = 0;
	timer_flag = 0;
	improv = 0;

	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

//从内核时间表中删除描述符
void removefd(int epollfd, int fd) {
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close) {
	if (real_close && (m_sockfd != -1)) {
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--;
	}
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
	epoll_event event;
	event.data.fd = fd;

	if (1 == TRIGMode) {
		event.events = ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
	}
	else {
		event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
	}

	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::initmysql_result(sql_connection_pool* connPool) {
	//先从连接池取一个连接
	MYSQL* mysql = NULL;
	connectionRAII mysqlcon(&mysql, connPool);

	if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
		LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
	}

	//从表中检索完整的结果集
	MYSQL_RES* result = mysql_store_result(mysql);

	//返回结果集的列数
	int num_fields = mysql_num_fields(result);

	//返回所有字段结构的数组
	MYSQL_FIELD* fields = mysql_fetch_fields(result);

	//从结果集中获取下一行，将对应的用户名和密码，存入map
	while (MYSQL_ROW row = mysql_fetch_row(result)) {
		string temp1(row[0]);
		string temp2(row[1]);
		users[temp1] = temp2;
	}
}

bool http_conn::read_once() {
	if (m_read_idx >= READ_BUFFER_SIZE) {
		return false;
	}
	int bytes_read = 0;

	//LT读数据
	if (0 == m_TRIGMode) {
		//读到的字节数返回到bytes_read
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		m_read_idx += bytes_read;

		//没读到数据
		if (bytes_read <= 0) {
			return false;
		}
		return true;
	}

	//ET读数据
	else {
		printf("ET read\n");
		while (true) {
			bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
			if (bytes_read == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					break;
				}
				return false;
			}

			else if (bytes_read == 0) {
				return false;
			}
			m_read_idx += bytes_read;
		}
		printf("%s\n", m_read_buf);
		return true;
	}
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
	m_url = strpbrk(text, " \t");
	if (!m_url) {
		return BAD_REQUEST;
	}
	*m_url++ = '\0';
	char* method = text;
	if (strcasecmp(method, "GET") == 0) {
		m_method = GET;
	}
	else if (strcasecmp(method, "POST") == 0) {
		m_method = POST;
		cgi = 1;
	}
	else {
		return BAD_REQUEST;
	}

	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if (!m_version) {
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if (strcasecmp(m_version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}

	//判断是否含有http://
	if (strncasecmp(m_url, "http://", 7) == 0) {
		m_url += 7;
		m_url = strchr(m_url, '/');
	}
	
	//判断是否含有https://
	if (strncasecmp(m_url, "https://", 8) == 0) {
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	if (!m_url || m_url[0] != '/') {
		return BAD_REQUEST;
	}
	if (strlen(m_url) == 1) {
		strcat(m_url, "judge.html");
	}

	m_check_state = CHECK_STATE_HEAD;
	return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
	if (text[0] == '\0') {
		if (m_content_length != 0) {
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0) {
			m_linger = true;
		}
	}

	else if (strncasecmp(text, "Content-length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}

	else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 5;
		text += strspn(text, " \t");
		m_host = text;
	}

	else {
		LOG_INFO("oop!unknow header:%s",text);
	}
	
	return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
	if (m_read_idx >= (m_content_length + m_checked_idx)) {
		text[m_content_length] = '\0';

		//POST请求中最后为输入的用户名和密码
		m_string = text;
		return GET_REQUEST;
	}
	return NO_REQUEST;
}


http_conn::HTTP_CODE http_conn::do_request() {
	
	//将初始化的m_real_file赋为网站根目录
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);

	//找到m_url中/的位置
	const char* p = strchr(m_url, '/');

	//处理cgi
	if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {

		//根据标志判断是登陆检测还是注册检测
		char flag = m_url[1];

		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/");
		strcat(m_real_url, m_url + 2);
		strncpy(m_real_file + len, m_real_url, FILENAME_LEN - len - 1);
		free(m_real_url);
	
		//将用户名和密码提取出来
		char name[100], password[100];

		int i;
		for (i = 5; m_string[i] != '&'; i++) {
			name[i - 5] = m_string[i];
		}
		name[i - 5] = '\0';

		int j =0;
		for (i = i + 10; m_string[j] != '\0'; i++, j++) {
			password[j] = m_string[i];
		}
		password[j] = '\0';

		if (*(p + 1) == '3') {
			//如果是注册，先检测数据库是否有重名
			//没有重名，进行增加数据
			char* sql_insert = (char*)malloc(sizeof(char)*200);
			strcpy(sql_insert, "INSERT INTO user(username,passwd) VALUES(");
			strcat(sql_insert, "'");
			strcat(sql_insert, name);
			strcat(sql_insert, "','");
			strcat(sql_insert, password);
			strcat(sql_insert, "')");

			if (users.find(name) == users.end()) {
				m_lock.lock();
				int res = mysql_query(mysql, sql_insert);
				users.insert(pair<string, string>(name, password));
				m_lock.unlock();

				if (!res) {
					strcpy(m_url, "/log.html");
				}
				else {
					strcpy(m_url, "/registerError.html");
				}
			}
			else
				strcpy(m_url, "/registerError.html");
		
		}

		//如果是登录，直接判断
		//若浏览器输入的用户名和密码何以查找到，返回1，否则返回0
		else if (*(p + 1) == '2') {
			//密码存在并且，与取出的password相等，即可以查到用户
			//直接登录
			if (users.find(name) != users.end() &&
				users[name] == password) 
			{
				strcpy(m_url, "/welcome.html");
			}
			
			//找不到用户
			else
			{
				strcpy(m_url, "/logError.html");
			}
		}

		printf("%s\n", m_real_file);
	}
	
	//如果请求资源为/0,表示跳转注册界面
	if (*(p + 1) == '0') {
		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/register.html");
		strncpy(m_real_file + len, m_real_url, strlen(m_real_url));

		free(m_real_url);
	}

	//跳转登陆界面
	else if (*(p + 1) == '1') {
		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/log.html");
		strncpy(m_real_file + len, m_real_url, strlen(m_real_url));

		free(m_real_url);
	}

	//跳转请求图片界面
	else if (*(p + 1) == '5') {
		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/picture.html");
		strncpy(m_real_file + len, m_real_url, strlen(m_real_url));

		free(m_real_url);
	}

	//跳转请求视频界面
	else if (*(p + 1) == '6') {
		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/video.html");
		strncpy(m_real_file + len, m_real_url, strlen(m_real_url));

		free(m_real_url);
	}

	//跳转界面
	else if (*(p + 1) == '7') {
		char* m_real_url = (char*)malloc(sizeof(char) * 200);
		strcpy(m_real_url, "/JoinUs.html");
		strncpy(m_real_file + len, m_real_url, strlen(m_real_url));

		free(m_real_url);
	}

	//以上均不符合，既不是登录也不是注册，直接将url与网站目录拼接
	//
	else {
		strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

	}

	//通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
	//失败返回NO_RESOURCE,表示资源不存在
	if (stat(m_real_file, &m_file_stat) < 0) {
		return NO_RESOURCE;
	}
	
	//判断文件的权限，是否可读
	if (!(m_file_stat.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}

	//判断文件的类型，如果是目录，返回BAD_REQUEST
	if (S_ISDIR(m_file_stat.st_mode)) {
		return BAD_REQUEST;
	}

	printf("%s\n", m_real_file);

	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ
		, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
	
}

void http_conn::unmap() {
	if (m_file_address) {
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}




//从状态机，分析一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() {
	char temp;
	for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
		temp = m_read_buf[m_checked_idx];
		if (temp == '\r') {
			if ((m_checked_idx + 1) == m_read_idx) {
				return LINE_OPEN;
			}
			else if (m_read_buf[m_checked_idx + 1] == '\n') {
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if (temp == '\n') {
			if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::process_read() {
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
		(line_status = parse_line()) == LINE_OK)
	{
		text = get_line();
		m_start_line = m_checked_idx;
		LOG_INFO("%s",text);

		switch (m_check_state) {
			case CHECK_STATE_REQUESTLINE:
			{
				ret = parse_request_line(text);
				if (ret == BAD_REQUEST) {
					return BAD_REQUEST;
				}
				break;
			}
			case CHECK_STATE_HEAD:
			{
				ret = parse_headers(text);
				if (ret == BAD_REQUEST) {
					return BAD_REQUEST;
				}
				else if (ret == GET_REQUEST) {
					return do_request();
				}
				break;
			}
			case CHECK_STATE_CONTENT:
			{
				ret = parse_content(text);
				if (ret == GET_REQUEST) {
					return do_request();
				}
				line_status = LINE_OPEN;
				break;
			}
			default:
			{
				return INTERNAL_ERROR;
			}
		}
	}
	return NO_REQUEST;
}

bool http_conn::add_response(const char* format, ...) {
	if (m_write_idx >= WRITE_BUFFER_SIZE) {
		return false;
	}

	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, format, arg_list);
	if (len >= (WRITE_BUFFER_SIZE - m_write_idx - 1)) {
		va_end(arg_list);
		return false;
	}
	m_write_idx += len;
	va_end(arg_list);

	LOG_INFO("request:%s",m_write_buf);

	return true;
}

bool http_conn::add_status_line(int status, const char* title) {
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_length) {
	return add_content_length(content_length) && add_linger() &&
		add_blank_line();
}

bool http_conn::add_content_length(int content_length) {
	return add_response("Content-Length:%d\r\n", content_length);
}

bool http_conn::add_linger() {
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line() {
	return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content) {
	return add_response("%s", content);
}




bool http_conn::process_write(HTTP_CODE ret) {
	switch (ret) {
	
		//内部错误，500
		case INTERNAL_ERROR:
		{	
			//状态行
			add_status_line(500, error_500_title);
			
			//消息报头
			add_headers(strlen(error_500_form));
			if (!add_content(error_500_form))
			{

				return false;
			}
			break;
		}

		//语法错误，404
		case BAD_REQUEST:
		{
			add_status_line(404, error_404_title);
			add_headers(strlen(error_404_form));
			if (!add_content(error_404_form)) {

				return false;
			}
			break;
		}

		//资源没有访问权限，403
		case FORBIDDEN_REQUEST:
		{
			add_status_line(403, error_403_title);
			add_headers(strlen(error_403_form));
			if (!add_content(error_403_form)) {

				return false;
			}
			break;

		}

		//文件存在，200
		case FILE_REQUEST:
		{
			add_status_line(200, ok_200_title);
			//如果请求资源存在
			if (m_file_stat.st_size != 0) {
				add_headers(m_file_stat.st_size);
				//第一个iovec指针指向响应报文缓冲区，长度m_write_idx
				m_iv[0].iov_base = m_write_buf;
				m_iv[0].iov_len = m_write_idx;
				//第二个iover指针指向mmap返回的文件指针，长度指向文件大小
				m_iv[1].iov_base = m_file_address;
				m_iv[1].iov_len = m_file_stat.st_size;
				m_iv_count = 2;
				bytes_to_send = m_write_idx + m_file_stat.st_size;
				return true;
			}
			else {
				const char* ok_string = "<html><body></body></html>";
				add_headers(strlen(ok_string));
				if (!add_content(ok_string)) {

					return false;
				}
			}
		}
		default:
		{
			return false;
		}

		m_iv[0].iov_base = m_write_buf;
		m_iv[0].iov_len = m_write_idx;
		m_iv_count = 1;
		bytes_to_send = m_write_idx;
		return true;
	}
}

bool http_conn::write() {
	int temp = 0;

	//如果要发送的数据长度为0
	//表示相应报文为空，一般不会出现这种情况
	if (bytes_to_send == 0) {
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
		init();
		return true;
	}

	while (true) {
		//将响应报文的状态行、消息头、空行和响应正文发送给浏览器
		temp = writev(m_sockfd, m_iv, m_iv_count);

		//发送失败
		if (temp < 0) {
			if (errno == EAGAIN) {
				modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
				return true;
			}

			unmap();
			return false;
		}

		bytes_have_send += temp;
		bytes_to_send -= temp;
		//第一个iovec头部信息的数据已发送完
		if (bytes_have_send >= m_iv[0].iov_len) {
			//不再继续发送头部信息
			m_iv[0].iov_len = 0;
			m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
			m_iv[1].iov_len = bytes_to_send;
		}
		else {
			m_iv[0].iov_base = m_write_buf + bytes_have_send;
			m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
		}

		if (bytes_to_send <= 0) {
			unmap();
			modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

			if (m_linger) {
				init();
				return true;
			}
			else {
				return false;
			}
		}
	}
}

void http_conn::process() {
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST) {
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
		return;
	}


	bool write_ret = process_write(read_ret);
	if (!write_ret) {
		close_conn();
	}
	modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}