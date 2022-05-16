#include "Promise.h"
#include "Loop.h"

#include <stdexcept>

void Promise::resolve(const std::any &value) {
	if (m_resolved)
		throw std::logic_error("Promise already resolved");
	
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

void Promise::runCallback(const CallbackItem &callback) {
	Loop::setTimeout([callback, this]() {
		auto result = callback.func(m_result);
		if (isOwnPtr(result)) {
			Ptr next_promise = getPtr(result);
			next_promise->then([promise = callback.promise](const std::any &value) {
				promise->resolve(value);
				return nullptr;
			});
		} else {
			callback.promise->resolve(result);
		}
	}, 0);
}

Promise::Ptr Promise::then(const Callback &func) {
	auto promise = make();
	if (m_resolved) {
		runCallback({func, promise});
	} else {
		m_callbacks.push_back({func, promise});
	}
	return promise;
}

Promise::Ptr Promise::all(const std::vector<Ptr> &promises) {
	auto multi = std::make_shared<PromiseMulti>(promises.size());
	
	int index = 0;
	for (auto &promise: promises) {
		promise->then([multi, index](const std::any &result) {
			multi->resolve(index, result);
			return nullptr;
		});
		index++;
	}
	
	return multi->promise();
}
