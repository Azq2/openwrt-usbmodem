#pragma once

#include <mutex>
#include <map>
#include <list>
#include <memory>
#include <functional>

#include "Log.h"
#include "Utils.h"

class LoopBase {
	protected:
		enum TimerFlags {
			TIMER_LOOP		= 1 << 0,
			TIMER_INSTALLED	= 1 << 1,
			TIMER_CANCEL	= 1 << 2
		};
		
		struct Timer {
			std::list<std::shared_ptr<Timer>>::iterator it;
			int id = 0;
			std::function<void()> callback;
			uint8_t flags = 0;
			int interval = 0;
			int64_t time = 0;
		};
		
		int m_waker_r = -1;
		int m_waker_w = -1;
		
		std::list<std::shared_ptr<Timer>> m_list;
		std::map<int, std::shared_ptr<Timer>> m_timers;
		std::mutex m_mutex;
		
		bool m_inited = false;
		bool m_need_stop = false;
		int m_global_timer_id = 0;
	
	protected:
		void addTimerToQueue(std::shared_ptr<Timer> new_timer);
		void removeTimerFromQueue(std::shared_ptr<Timer> timer);
		
		void runTimeouts();
		void handlerSignal(int sig);
		
		virtual void implInit() = 0;
		virtual void implSetNextTimeout(int64_t time) = 0;
		virtual void implRun() = 0;
		virtual void implStop() = 0;
		virtual void implDestroy() = 0;
		
		void done();
		void wake();
	public:
		~LoopBase() {
			destroy();
		}
		
		void init();
		void run();
		void stop();
		void destroy();
		
		int addTimer(const std::function<void()> &callback, int timeout_ms, bool loop);
		void removeTimer(int id);
};
