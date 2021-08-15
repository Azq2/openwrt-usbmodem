#include "Loop.h"

#include <fcntl.h>
#include <csignal>

Loop *Loop::m_instance = nullptr;

Loop::Loop() {
	
}

Loop::~Loop() {
	_done();
}

void Loop::handlerSignal(int sig) {
	LOGD("Received signal: %d\n", sig);
	m_need_stop = true;
	
	if (m_uloop_inited) {
		while (write(m_waker_w, "w", 1) < 0 && errno == EINTR);
	}
}

bool Loop::_init() {
	// Init signals
	auto handler = +[](int sig) {
		instance()->handlerSignal(sig);
	};
	std::signal(SIGINT, handler);
	std::signal(SIGTERM, handler);
	
	// Init uloop
	if (uloop_init() < 0) {
		LOGE("init() failed\n");
		return false;
	}
	
	m_main_timeout.cb = uloopMainTimeoutHandler;
	
	int fds[2];
	
	if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) < 0) {
		LOGE("pipe() failed, error = %d\n", errno);
		_done();
		return false;
	}
	
	m_waker_r.fd = fds[0];
	m_waker_r.cb = uloopWakerHandler;
	
	if (uloop_fd_add(&m_waker_r, ULOOP_READ) < 0) {
		LOGE("uloop_fd_add() failed\n");
		_done();
		return false;
	}
	
	m_waker_w = fds[1];
	
	m_uloop_inited = true;
	
	return true;
}

void Loop::_run() {
	if (!m_need_stop)
		uloop_run();
	_done();
}

void Loop::_done() {
	if (m_waker_w >= 0) {
		uloop_fd_delete(&m_waker_r);
		close(m_waker_w);
		close(m_waker_r.fd);
		m_waker_w = -1;
	}
	
	if (m_uloop_inited) {
		uloop_done();
		m_uloop_inited = false;
	}
	m_timers.clear();
}

void Loop::_stop() {
	if (m_waker_w >= 0) {
		uloop_end();
		while (write(m_waker_w, "w", 1) < 0 && errno == EINTR);
	}
}

void Loop::on(size_t event_id, const std::function<void(const std::any &event)> &callback) {
	m_mutex.lock();
	m_events[event_id].push_back(callback);
	m_mutex.unlock();
}

void Loop::_emit(const std::any &value) {
	setTimeout([this, value]() {
		auto handlers = m_events.find(value.type().hash_code());
		if (handlers != m_events.end()) {
			for (auto &callback: handlers->second)
				callback(value);
		}
	}, 0);
}

void Loop::uloopMainTimeoutHandler(uloop_timeout *timeout) {
	instance()->runNextTimeout();
}

void Loop::uloopWakerHandler(struct uloop_fd *fd, unsigned int events) {
	static char buf[4];
	while (read(fd->fd, buf, sizeof(buf)) > 0 || errno == EINTR);
	instance()->runNextTimeout();
}

void Loop::runNextTimeout() {
	if (m_need_stop) {
		uloop_end();
		return;
	}
	
	m_mutex.lock();
	Timer *first_timer = !list_empty(&timeouts) ? reinterpret_cast<Timer *>(timeouts.next) : nullptr;
	m_mutex.unlock();
	
	if (!first_timer)
		return;
	
	if (first_timer->time - getCurrentTimestamp() <= 0) {
		if (!(first_timer->flags & TIMER_CANCEL))
			first_timer->callback();
		
		if ((first_timer->flags & TIMER_LOOP) && !(first_timer->flags & TIMER_CANCEL)) {
			m_mutex.lock();
			first_timer->time = getCurrentTimestamp() + first_timer->interval;
			addTimerToQueue(first_timer);
			m_mutex.unlock();
		} else {
			m_mutex.lock();
			removeTimerFromQueue(first_timer);
			m_timers.erase(first_timer->id);
			m_mutex.unlock();
		}
		
		m_mutex.lock();
		first_timer = !list_empty(&timeouts) ? reinterpret_cast<Timer *>(timeouts.next) : nullptr;
		m_mutex.unlock();
	}
	
	if (first_timer) {
		int wait = first_timer->time - getCurrentTimestamp();
		uloop_timeout_set(&m_main_timeout, wait < 0 ? 0 : wait);
	}
}

void Loop::removeTimerFromQueue(Timer *new_timer) {
	if ((new_timer->flags & TIMER_INSTALLED)) {
		list_del(&new_timer->list);
		new_timer->flags &= ~TIMER_INSTALLED;
	}
}

void Loop::removeTimer(int id) {
	if (!m_uloop_inited)
		return;
	
	m_mutex.lock();
	auto it = m_timers.find(id);
	if (it != m_timers.end())
		it->second.flags |= TIMER_CANCEL;
	m_mutex.unlock();
}

void Loop::addTimerToQueue(Timer *new_timer) {
	list_head *cursor = &timeouts;
	Timer *timer;
	
	removeTimerFromQueue(new_timer);
	
	list_for_each_entry(timer, &timeouts, list) {
		if (timer->time - new_timer->time > 0) {
			cursor = &timer->list;
			break;
		}
	}
	
	list_add_tail(&new_timer->list, cursor);
	
	new_timer->flags |= TIMER_INSTALLED;
}

int Loop::addTimer(const std::function<void()> &callback, int timeout_ms, bool loop) {
	if (!m_uloop_inited)
		return -1;
	
	m_mutex.lock();
	int id = m_global_timer_id++;
	
	Timer *new_timer = &m_timers[id];
	new_timer->id = id;
	new_timer->interval = timeout_ms;
	new_timer->callback = callback;
	new_timer->time = getCurrentTimestamp() + timeout_ms;
	new_timer->flags = loop ? TIMER_LOOP : 0;
	
	addTimerToQueue(new_timer);
	
	m_mutex.unlock();
	
	while (write(m_waker_w, "w", 1) < 0 && errno == EINTR);
	
	return id;
}
