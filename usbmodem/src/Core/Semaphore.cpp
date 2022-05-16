#include "Semaphore.h"
#include "Utils.h"

#include <cerrno>
#include <stdexcept>

Semaphore::Semaphore() {
	if (sem_init(&m_sem, 0, 0) != 0)
		throw std::runtime_error("sem_init fatal error");
}

void Semaphore::post() {
	int ret;
	do {
		ret = sem_post(&m_sem);
	} while (ret < 0 && errno == EINTR);
	
	if (ret != 0)
		throw std::runtime_error("sem_post fatal error");
}

bool Semaphore::wait(int64_t timeout) {
	int64_t start = getCurrentTimestamp();
	struct timespec tm;
	int ret;
	do {
		setTimespecTimeout(&tm, getNewTimeout(start, timeout));
		ret = sem_timedwait(&m_sem, &tm);
	} while (ret == -1 && errno == EINTR);
	
	if (ret == -1) {
		if (errno == ETIMEDOUT)
			return false;
		
		throw std::runtime_error("sem_timedwait fatal error");
	}
	
	return true;
}
