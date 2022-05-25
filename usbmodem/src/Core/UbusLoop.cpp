#include "UbusLoop.h"

#include <fcntl.h>
#include <stdexcept>

UbusLoop *UbusLoop::m_instance = nullptr;

const char *UbusLoop::name() {
	return "UbusLoop";
}

void UbusLoop::implInit() {
	if (uloop_init() < 0)
		throw std::runtime_error("uloop_init error");
	
	m_main_timeout.cb = +[](uloop_timeout *timeout) {
		instance()->runTimeouts();
	};
	
	m_waker.fd = m_waker_r;
	m_waker.cb = +[](struct uloop_fd *ufd, unsigned int events) {
		static char buf[4];
		while (read(ufd->fd, buf, sizeof(buf)) > 0 || errno == EINTR);
		instance()->runTimeouts();
	};
	
	if (uloop_fd_add(&m_waker, ULOOP_READ) < 0)
		throw std::runtime_error("uloop_fd_add error");
}

void UbusLoop::implSetNextTimeout(int64_t time) {
	int wait = time - getCurrentTimestamp();
	uloop_timeout_set(&m_main_timeout, wait < 0 ? 0 : wait);
}

void UbusLoop::implRun() {
	uloop_run();
}

void UbusLoop::implRequestStop() {
	uloop_end();
}

void UbusLoop::implStop() {
	
}

void UbusLoop::implDestroy() {
	uloop_fd_delete(&m_waker);
	uloop_done();
}

UbusLoop *UbusLoop::instance() {
	if (!m_instance)
		m_instance = new UbusLoop();
	return m_instance;
}
