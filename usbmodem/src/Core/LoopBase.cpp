#include "LoopBase.h"

#include <fcntl.h>
#include <unistd.h>
#include <csignal>

void LoopBase::init() {
	int fds[2];
	
	if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) < 0)
		throw std::runtime_error("pipe2() error");
	
	m_waker_r = fds[0];
	m_waker_w = fds[1];
	
	implInit();
	
	m_inited = true;
}

void LoopBase::run() {
	m_thread_id = std::this_thread::get_id();
	
	if (!m_inited)
		return;
	
	if (!m_need_stop) {
		m_run = true;
		implRun();
		m_run = false;
	}
	
	for (auto &promise: m_promises)
		promise.set_value(PromiseTimeout({}));
	
	implStop();
}

void LoopBase::destroy() {
	if (m_inited) {
		implDestroy();
		
		close(m_waker_w);
		close(m_waker_r);
		
		m_list.clear();
		m_timers.clear();
		
		m_inited = false;
	}
}

void LoopBase::stop() {
	m_need_stop = true;
	implRequestStop();
}

void LoopBase::runTimeouts() {
	if (m_need_stop)
		return;
	
	m_mutex.lock();
	auto next_timer = (m_list.size() > 0 ? m_list.front() : nullptr);
	m_mutex.unlock();
	
	if (!next_timer) {
		implSetNextTimeout(getCurrentTimestamp() + 60000);
		return;
	}
	
	if (next_timer->time - getCurrentTimestamp() <= 0) {
		if (!(next_timer->flags & TIMER_CANCEL))
			next_timer->callback();
		
		if ((next_timer->flags & TIMER_LOOP) && !(next_timer->flags & TIMER_CANCEL)) {
			m_mutex.lock();
			next_timer->time = getCurrentTimestamp() + next_timer->interval;
			addTimerToQueue(next_timer);
			m_mutex.unlock();
		} else {
			m_mutex.lock();
			removeTimerFromQueue(next_timer);
			m_timers.erase(next_timer->id);
			m_mutex.unlock();
		}
		
		m_mutex.lock();
		next_timer = (m_list.size() > 0 ? m_list.front() : nullptr);
		m_mutex.unlock();
	}
	
	if (next_timer) {
		implSetNextTimeout(next_timer->time);
	} else {
		implSetNextTimeout(getCurrentTimestamp() + 60000);
	}
}

void LoopBase::addTimerToQueue(std::shared_ptr<Timer> new_timer) {
	removeTimerFromQueue(new_timer);
	
	bool inserted = false;
	for (auto it = m_list.begin(); it != m_list.end(); it++) {
		if (it->get()->time - new_timer->time > 0) {
			new_timer->it = m_list.insert(it, new_timer);
			inserted = true;
			break;
		}
	}
	
	if (!inserted) {
		m_list.push_back(new_timer);
		new_timer->it = --m_list.end();
	}
	
	new_timer->flags |= TIMER_INSTALLED;
}

void LoopBase::removeTimerFromQueue(std::shared_ptr<Timer> timer) {
	if ((timer->flags & TIMER_INSTALLED)) {
		m_list.erase(timer->it);
		timer->flags &= ~TIMER_INSTALLED;
	}
}

void LoopBase::wake() {
	if (m_waker_w != -1) {
		while (write(m_waker_w, "w", 1) < 0 && errno == EINTR);
	}
}

int LoopBase::addTimer(const std::function<void()> &callback, int timeout_ms, bool loop) {
	if (!m_inited)
		return -1;
	
	m_mutex.lock();
	
	int id = m_global_timer_id++;
	
	auto new_timer = std::make_shared<Timer>();
	new_timer->id = id;
	new_timer->interval = timeout_ms;
	new_timer->callback = callback;
	new_timer->time = getCurrentTimestamp() + timeout_ms;
	new_timer->flags = loop ? TIMER_LOOP : 0;
	
	m_timers[id] = new_timer;
	addTimerToQueue(new_timer);
	
	m_mutex.unlock();
	
	wake();
	
	return id;
}

std::any LoopBase::execOnThisThread(const std::function<std::any()> &callback) {
	if (!checkThreadId()) {
		m_mutex.lock();
		m_promises.resize(m_promises.size() + 1);
		auto &promise = m_promises.back();
		auto promise_it = --m_promises.end();
		m_mutex.unlock();
		
		addTimer([this, &promise, &callback]() {
			promise.set_value(callback());
		}, 0, false);
		
		auto future = promise.get_future();
		future.wait();
		auto value = future.get();
		
		m_mutex.lock();
		m_promises.erase(promise_it);
		m_mutex.unlock();
		
		if (value.type() == typeid(PromiseTimeout))
			return {};
		
		return value;
	} else {
		return callback();
	}
}

void LoopBase::removeTimer(int id) {
	if (!m_inited)
		return;
	
	m_mutex.lock();
	auto it = m_timers.find(id);
	if (it != m_timers.end())
		it->second->flags |= TIMER_CANCEL;
	m_mutex.unlock();
}
