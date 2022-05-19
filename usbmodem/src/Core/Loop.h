#pragma once

#include "Log.h"
#include "Utils.h"
#include "LoopBase.h"

class Loop: public LoopBase {
	protected:
		static Loop *m_instance;
		int64_t m_next_run = 0;
		
		void implInit() override;
		void implSetNextTimeout(int64_t time) override;
		void implRun() override;
		void implStop() override;
		void implDestroy() override;
	
	public:
		static Loop *instance();
		
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
