#pragma once

#include <map>
#include <any>
#include <mutex>
#include <string>
#include <functional>

class Events {
	public:
		typedef std::function<void(const std::any &event)> EventCallback;
		typedef std::map<size_t, std::vector<EventCallback>> EventsStorage;
	protected:
		EventsStorage m_events;
		std::mutex m_mutex;
	public:
		void emit(const std::any &value);
		void on(size_t event_id, const EventCallback &callback);
		
		template <typename T>
		inline void on(const std::function<void(const T &)> &callback) {
			on(typeid(T).hash_code(), [callback](const std::any &event) {
				callback(std::any_cast<T>(event));
			});
		}
};
