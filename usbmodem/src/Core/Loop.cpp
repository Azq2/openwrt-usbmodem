#include "Loop.h"

#include <fcntl.h>
#include <poll.h>
#include <csignal>
#include <stdexcept>

Loop *Loop::m_instance = nullptr;

void Loop::implInit() {
	auto handler = +[](int sig) {
		LOGD("Received signal: %d\n", sig);
		instance()->stop();
	};
	std::signal(SIGINT, handler);
	std::signal(SIGTERM, handler);
}

void Loop::implSetNextTimeout(int64_t time) {
	m_next_run = time;
}

void Loop::implRun() {
	static char buf[4];
	struct pollfd pfd[] = {{.fd = m_waker_r, .events = POLLIN}};
	
	while (!m_need_stop) {
		while (read(m_waker_r, buf, sizeof(buf)) > 0 || errno == EINTR);
		
		runTimeouts();
		
		int ret;
		do {
			int timeout = m_next_run - getCurrentTimestamp();
			if (timeout <= 0 || m_need_stop)
				break;
			
			ret = ::poll(pfd, 1, timeout);
		} while (!m_need_stop && ret < 0 && errno == EINTR);
		
		if (ret < 0 && errno != EINTR)
			throw std::runtime_error("poll error");
	}
}

void Loop::implStop() {
	
}

void Loop::implDestroy() {
	
}

Loop *Loop::instance() {
	if (!m_instance)
		m_instance = new Loop();
	return m_instance;
}
