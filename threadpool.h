#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

#include "lock.h"
#include "sql_connection_pool.h"

// �̳߳��� �����ģ������Ϊ�˴��븴�ã�ģ�����T��������
template<typename T>
class threadpool {
public:
	threadpool(int actor_model, sql_connection_pool *connPool,
		int thread_number = 8, int max_requests = 10000);
	~threadpool();


private:
	int m_actor_model;					//ģ��
	int m_thread_number;				//�̳߳��е��߳���
	int m_max_requests;					//�����������������������
	pthread_t* m_threads;				//�̳߳ص����飬���СΪm_thread_number
	sql_connection_pool* m_connPool;		//���ݿ�
	int actor_model;					//ģ��

	locker m_queuelocker;				//�������еĻ�����
	std::list<T*> m_workqueue;			//��������
	sem m_queuestat;					//�ź������ж��Ƿ�������

	static void* worker(void *arg);
	void run();
	
};

template <typename T>
threadpool<T>::threadpool(int actor_model, sql_connection_pool* connPool, int thread_number, int max_requests) :
	m_actor_model(actor_model),
	m_connPool(connPool),
	m_thread_number(thread_number),
	m_max_requests(max_requests),
	m_threads(NULL)
{
	if (thread_number <= 0 || max_requests <= 0) {
		throw std::exception();
	}

	m_threads = new pthread_t[m_thread_number];

	if (!m_threads) {
		throw std::exception();
	}

	for (int i = 0; i < m_thread_number; i++) {
		if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
			delete[] m_threads;
			throw std::exception();
		}

		if (pthread_detach(m_threads[i])) {
			delete[] m_threads;
				throw std::exception();
		}
	}
}

template <typename T>
void* threadpool<T>::worker(void* arg) {
	threadpool* pool = (threadpool*)arg;
	pool->run();

	return pool;
}

template <typename T>
void threadpool<T>::run() {
	while (true) {
		
		m_queuestat.wait();
		m_queuelocker.lock();
		if (m_workqueue.empty()) {
			//�������������Ϊ�գ��ͷ���,continue
			m_queuelocker.unlock();
			continue;
		}

		//�����Ϊ�գ��Ͱ�����ȡ�������ٸ���ģ��ѡ����ģʽ (ET or LT)
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();


		
		if (!request) {
			continue;
		}

		//�ж�ģʽ
		if (1 == m_actor_model) {
		
		
		}

		else {
			
		}



	}
}


template <typename T>
threadpool<T>::~threadpool() {
	delete[] m_threads;
}

#endif