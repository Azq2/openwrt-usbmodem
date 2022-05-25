#include "Loop.h"

#include <fcntl.h>
#include <poll.h>
#include <stdexcept>

Loop *Loop::m_instance = nullptr;

const char *Loop::name() {
	return "Loop";
}

void Loop::implInit() {
	
}

void Loop::implSetNextTimeout(int64_t time) {
	m_next_run = time;
}

void Loop::implRun() {
	static char buf[4];
	struct pollfd pfd[] = {{.fd = m_waker_r, .events = POLLIN}};
	
	while (!m_need_stop) {
		LOGD("[Loop] m_need_stop=%d\n", m_need_stop);
		
		runTimeouts();
		
		int timeout = m_next_run - getCurrentTimestamp();
		if (timeout > 0) {
			LOGD("[Loop] poll %d\n", timeout);
			int ret = ::poll(pfd, 1, timeout);
			LOGD("[Loop] poll end\n");
			if (ret < 0 && errno != EINTR) {
				LOGE("poll errno = %d\n", errno);
				throw std::runtime_error("poll error");
			}
			
			if ((pfd[0].revents & (POLLERR | POLLHUP))) {
				LOGE("poll POLLERR or POLLHUP\n");
				throw std::runtime_error("poll error");
			}
	
			if ((pfd[0].revents & POLLIN)) {
				while (read(m_waker_r, buf, sizeof(buf)) > 0 || errno == EINTR);
			}
		}
	}
	
	LOGD("Loop done\n");
}

void Loop::implStop() {
	
}

void Loop::implRequestStop() {
	wake();
}

void Loop::implDestroy() {
	
}

Loop *Loop::instance() {
	if (!m_instance)
		m_instance = new Loop();
	return m_instance;
}
