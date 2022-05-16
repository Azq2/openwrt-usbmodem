#pragma once

#include <variant>
#include <functional>
#include <memory>
#include <any>

#include "Log.h"
#include "Utils.h"

class Promise {
	public:
		typedef std::shared_ptr<Promise> Ptr;
		
		typedef std::function<std::any(const std::any &)> Callback;
		typedef std::function<std::any()> CallbackNoResult;
		
		struct CallbackItem {
			Callback func;
			Ptr promise;
		};
	protected:
		std::vector<CallbackItem> m_callbacks;
		std::any m_result;
		bool m_resolved = false;
		Ptr m_self;
		
		void runCallback(const CallbackItem &callback);
		
		virtual bool isOwnPtr(const std::any &value) {
			return value.type() == typeid(Ptr);
		}
		
		virtual Promise::Ptr getPtr(const std::any &value) {
			return std::any_cast<Ptr>(value);
		}
	public:
		inline bool resolved() {
			return m_resolved;
		}
		
		void resolve(const std::any &value = nullptr);
		
		template <typename T>
		inline void resolve(const T &value) {
			resolve(std::any(value));
		}
		
		Ptr then(const Callback &callback);
		
		inline Ptr then(const CallbackNoResult &callback) {
			return then([callback](const std::any &value) {
				return callback();
			});
		}
		
		template <typename T>
		inline Ptr then(const std::function<std::any(const T &)> &callback) {
			return then([callback, this](const std::any &value) {
				if (typeid(T) != value.type())
					throw std::runtime_error(strprintf("Type mismatch, expected '%s' but got '%s'", typeid(T).name(), value.type().name()));
				return callback(std::any_cast<const T &>(value));
			});
		}
		
		template <typename T>
		inline T get() {
			return std::any_cast<T>(m_result);
		}
		
		static Ptr all(const std::vector<Ptr> &promises);
		
		static inline Ptr make() {
			auto ptr = std::make_shared<Promise>();
			ptr->m_self = ptr;
			return ptr;
		}
};

template <typename T>
class PromiseTyped: public Promise {
	public:
		typedef std::shared_ptr<PromiseTyped<T>> Ptr;
	
	protected:
		virtual bool isOwnPtr(const std::any &value) override {
			return Promise::isOwnPtr(value) || value.type() == typeid(Ptr);
		}
		
		virtual Promise::Ptr getPtr(const std::any &value) override {
			if (Promise::isOwnPtr(value))
				return Promise::getPtr(value);
			return std::any_cast<Ptr>(value);
		}
	public:
		inline void resolve(const T &value) {
			Promise::resolve(std::any(value));
		}
		
		inline T get() {
			return std::any_cast<T>(m_result);
		}
		
		static inline Ptr make() {
			auto ptr = std::make_shared<PromiseTyped<T>>();
			ptr->m_self = ptr;
			return ptr;
		}
};

class PromiseMulti {
	public:
		typedef std::vector<std::any> Result;
	
	protected:
		int m_counter = 0;
		Result m_results;
		Promise::Ptr m_promise = nullptr;
	
	public:
		PromiseMulti(int size) {
			m_promise = Promise::make();
			m_counter = size;
			m_results.resize(size);
			
			if (!m_counter)
				done();
		}
		
		inline void resolve(int i, const std::any &value) {
			m_counter--;
			m_results[i] = value;
			
			if (!m_counter)
				done();
		}
		
		inline Promise::Ptr promise() {
			return m_promise;
		}
		
		inline void done() {
			m_promise->resolve(m_results);
			m_results.clear();
		}
		
		inline const Result &getResults() {
			return m_results;
		}
};
