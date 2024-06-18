#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include "lock.h"

template <typename T>
class block_queue {
public:
	block_queue(int max_size = 1000) {
		if (max_size < 0) {
			exit(-1);
		}

		m_max_size = max_size;
		m_array = new T[max_size];
		m_size = 0;
		m_front = -1;
		m_back = -1;

	}

	~block_queue() {
		m_mutex.lock();
		if (NULL != m_array)
			delete[] m_array;

		m_mutex.unlock();
	}

	void clear() {
		m_mutex.lock();
		m_size = 0;
		m_front = -1;
		m_back = -1;
		m_mutex.unlock();
	}
	
	//�ж��Ƿ���
	bool is_full() {
		m_mutex.lock();
		if (m_size >= m_max_size) {
			m_mutex.unlock();
			return true;
		}

		m_mutex.unlock();
		return false;
	}

	//���ض���Ԫ��
	bool get_front(T &value) {
		m_mutex.lock();

		if (0 == m_size) {
			m_mutex.unlock();
			return false;
		}

		value = m_array[m_front];
		return true;
	
	}

	//���ض�βԪ��
	bool get_back(T& value) {
		m_mutex.lock();

		if (0 == m_size) {
			m_mutex.unlock();
			return false;
		}

		value = m_array[m_back];
		m_mutex.unlock();
		return true;
	}

	int get_size() {
		int tmp = 0;
		m_mutex.lock();
		tmp = m_size;

		m_mutex.unlock();
		return tmp;
	}

	int get_max_size() {
		int tmp = 0;
		m_mutex.lock();
		tmp = m_max_size;
		
		m_mutex.unlock();
		return tmp;
	}

	//�����������Ԫ�أ���Ҫ������ʹ�ö��е��߳��Ȼ���
	//����Ԫ��push�����У��൱��������������һ��Ԫ��
	//����ǰû���̵߳ȴ�����������������
	bool push(const T& item) {
		m_mutex.lock();
		if (m_size >= m_max_size) {
			m_cond.broadcast();
			m_mutex.unlock();
			return false;
		}

		m_back = (m_back + 1) % m_max_size;
		m_array[m_back] = item;

		m_size++;

		m_cond.broadcast();
		m_mutex.unlock();
		return true;
	}

	//popʱ�������ǰ����û��Ԫ�أ�����ȴ���������
	bool pop(T &item) {
		m_mutex.lock();
		while (m_size <= 0) {
			if (!m_cond.wait(m_mutex.get())) {
				m_mutex.unlock();
				return false;
			}
		}

		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];
		m_size--;
		m_mutex.unlock();
		return true;
	}

	//���ӳ�ʱ����
	bool pop(T& item, int ms_timeout) {
		struct timespec t = { 0,0 };
		struct timeval now = { 0,0 };
		gettimeofday(&now,NULL);
		m_mutex.lock();
		if (m_size <= 0) {
			t.tv_sec = now.tv_sec + ms_timeout / 1000;
			t.tv_nsec = (ms_timeout % 1000) * 1000;
			if (!m_cond.timewait(m_mutex.get(), t)) {
				m_mutex.unlock();
				return false;
			}
		}

		m_front = (m_front + 1) % m_max_size;
		item = m_array[m_front];
		m_size--;
		m_mutex.unlock();
		return true;
	}

private:
	locker m_mutex;
	cond m_cond;

	int m_max_size;
	int m_size;
	int m_front;
	int m_back;
	T* m_array;
};

#endif