#include "Deferred.h"
#include "Loop.h"

void DeferredMulti::wait(const std::vector<DeferredBase::Ptr> &deferreds, const std::function<void()> &callback) {
	int *counter = new int[0];
	counter[0] = deferreds.size();
	
	for (auto &deferred: deferreds) {
		deferred->then([counter, callback]() {
			counter[0]--;
			if (counter[0] == 0)
				callback();
		});
	}
}

void DeferredBase::runCallback(const Callback &callback) {
	Loop::setTimeout(callback, 0);
}

void DeferredBase::resolve(const std::any &value) {
	if (m_resolved)
		throw std::logic_error("Deferred already resolved");
	
	m_result = value;
	m_resolved = true;
	
	if (m_callbacks.size() > 0) {
		for (auto &callback: m_callbacks)
			runCallback(callback);
		m_callbacks.clear();
	}
	
	Loop::setTimeout([this]() {
		// Now we can destroy self
		m_self = nullptr;
	}, 0);
}

void DeferredBase::then(const Callback &callback) {
	if (m_resolved) {
		runCallback(callback);
	} else {
		m_callbacks.push_back(callback);
	}
}
