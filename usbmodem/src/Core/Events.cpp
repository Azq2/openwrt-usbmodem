#include "Events.h"
#include "Loop.h"

void Events::emit(const std::any &value) {
	Loop::setTimeout([this, value]() {
		auto handlers = m_events.find(value.type().hash_code());
		if (handlers != m_events.end()) {
			for (auto &callback: handlers->second)
				callback(value);
		}
	}, 0);
}

void Events::on(size_t event_id, const EventCallback &callback) {
	m_mutex.lock();
	m_events[event_id].push_back(callback);
	m_mutex.unlock();
}
