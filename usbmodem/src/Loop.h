#pragma once

#include <any>
#include <queue>
#include <mutex>
#include <map>
#include <functional>

extern "C" {
#include <libubox/list.h>
#include <libubox/uloop.h>
#include <libubus.h>
};

#include "Log.h"
#include "Utils.h"

class Loop {
	protected:
		enum TimerFlags {
			TIMER_LOOP		= 1 << 0,
			TIMER_INSTALLED	= 1 << 1,
			TIMER_CANCEL	= 1 << 2
		};
		
		struct Timer {
			// Must be a first item of struct!!!
			struct list_head list;
			
			int id;
			std::function<void()> callback;
			uint8_t flags;
			int interval;
			int64_t time;
		};
		
		Loop();
		~Loop();
		
		static Loop *m_instance;
		
		uloop_fd m_waker_r = {};
		int m_waker_w = -1;
		
		uloop_timeout m_main_timeout = {};
		
		list_head timeouts = LIST_HEAD_INIT(timeouts);
		
		std::map<int, Timer> m_timers;
		std::map<size_t, std::vector<std::function<void(const std::any &event)>>> m_events;
		
		std::mutex m_mutex;
		
		bool m_uloop_inited = false;
		int m_global_timer_id = 0;
		
		void addTimerToQueue(Timer *new_timer);
		static void removeTimerFromQueue(Timer *new_timer);
		
		int addTimer(const std::function<void()> &callback, int timeout_ms, bool loop);
		void removeTimer(int id);
		
		static void uloopMainTimeoutHandler(uloop_timeout *timeout);
		static void uloopWakerHandler(struct uloop_fd *fd, unsigned int events);
		
		void runNextTimeout();
		
		bool _init();
		void _run();
		void _done();
		
		void _emit(const std::any &value);
		void on(size_t event_id, const std::function<void(const std::any &event)> &callback);
		
		static bool initPipeFd(int fd);
	public:
		static inline Loop *instance() {
			if (!m_instance)
				m_instance = new Loop();
			return m_instance;
		}
		
		inline static bool init() {
			return instance()->_init();
		}
		
		inline static void run() {
			instance()->_run();
		}
		
		inline static void done() {
			instance()->_done();
		}
		
		template <typename T>
		static void on(const std::function<void(const T &)> &callback) {
			instance()->on(typeid(T).hash_code(), [callback](const std::any &event) {
				callback(std::any_cast<T>(event));
			});
		}
		
		static inline void emit(const std::any &value) {
			instance()->_emit(value);
		}
		
		template <typename T>
		static inline void emit(const T &value) {
			instance()->_emit(value);
		}
		
		static inline int setTimeout(const std::function<void()> &callback, int timeout_ms) {
			return instance()->addTimer(callback, timeout_ms, false);
		}
		
		static inline int setInterval(const std::function<void()> &callback, int timeout_ms) {
			return instance()->addTimer(callback, timeout_ms, true);
		}
		
		static inline void clearTimeout(int id) {
			instance()->removeTimer(id);
		}
		
		static inline void clearInterval(int id) {
			instance()->removeTimer(id);
		}
};
