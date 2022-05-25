#pragma once

#include "Log.h"
#include "Utils.h"
#include "LoopBase.h"

class Loop: public LoopBase {
	protected:
		static Loop *m_instance;
		int64_t m_next_run = 0;
		
		const char *name() override;
		void implInit() override;
		void implSetNextTimeout(int64_t time) override;
		void implRun() override;
		void implRequestStop() override;
		void implStop() override;
		void implDestroy() override;
	
	public:
		static Loop *instance();
		
		static inline void assertThread() {
			if (!instance()->checkThreadId())
				throw std::runtime_error("Invalid thread id (UbusLoop)");
		}
		
		static inline bool isOwnThread() {
			return instance()->checkThreadId();
		}
		
		template <typename T>
		static inline std::optional<T> exec(const std::function<T()> &callback) {
			auto value = instance()->execOnThisThread(callback);
			if (value.type() == typeid(void))
				return std::nullopt;
			return std::any_cast<T>(value);
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
