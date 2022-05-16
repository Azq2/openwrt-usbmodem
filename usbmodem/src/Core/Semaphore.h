#pragma once

#include <cstdint>
#include <semaphore.h>

class Semaphore {
	protected:
		sem_t m_sem = {};
	
	public:
		Semaphore();
		void post();
		bool wait(int64_t timeout);
};
