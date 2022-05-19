#pragma once

#include "Log.h"
#include "Utils.h"
#include "LoopBase.h"

extern "C" {
#include <libubox/list.h>
#include <libubox/uloop.h>
#include <libubus.h>
};

class UbusLoop: public LoopBase {
	protected:
		static UbusLoop *m_instance;
		uloop_timeout m_main_timeout = {};
		struct uloop_fd m_waker = {};
		
		void implInit() override;
		void implSetNextTimeout(int64_t time) override;
		void implRun() override;
		void implStop() override;
		void implDestroy() override;
	
	public:
		static UbusLoop *instance();
		
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
