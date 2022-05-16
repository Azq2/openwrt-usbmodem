#pragma once

#include <variant>
#include <functional>
#include <memory>
#include <any>

#include "Log.h"
#include "Utils.h"

class DeferredBase {
	public:
		typedef std::shared_ptr<DeferredBase> Ptr;
		typedef std::function<void()> Callback;
	
	protected:
		std::vector<Callback> m_callbacks;
		std::any m_result;
		bool m_resolved = false;
		Ptr m_self;
		
		void runCallback(const Callback &callback);
	
	public:
		inline bool resolved() {
			return m_resolved;
		}
		
		template <typename T>
		inline T get() {
			return std::any_cast<T>(m_result);
		}
		
		void resolve(const std::any &value);
		void then(const Callback &callback);
};

template <typename T>
class Deferred: public DeferredBase {
	public:
		typedef std::shared_ptr<Deferred<T>> Ptr;
		
		inline void resolve(const T &value) {
			DeferredBase::resolve(value);
		}
		
		inline void then(const Callback &callback) {
			DeferredBase::then(callback);
		}
		
		inline void then(const std::function<void(const T &)> &callback) {
			DeferredBase::then([this, callback]() {
				callback(get());
			});
		}
		
		inline T get() {
			return std::any_cast<T>(m_result);
		}
		
		static inline Ptr make() {
			auto ptr = std::make_shared<Deferred>();
			ptr->m_self = ptr;
			return ptr;
		}
};

class DeferredMulti {
	public:
		static void wait(const std::vector<DeferredBase::Ptr> &deferreds, const std::function<void()> &callback);
};
