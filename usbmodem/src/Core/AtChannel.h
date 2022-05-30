#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>

#include "Semaphore.h"
#include "Serial.h"
#include "Log.h"

class AtChannel {
	public:
		static const std::string empty_line;
		
		typedef std::function<int(const std::string &cmd)> TimeoutSetCallback;
		
		enum Errors {
			AT_SUCCESS		= 0,
			AT_TIMEOUT		= -1,
			AT_ERROR		= -2,
			AT_IO_ERROR		= -3,
			AT_IO_BROKEN	= -4
		};
		
		struct Response {
			Errors error;
			std::vector<std::string> lines;
			std::string status;
			
			const inline std::string &data() const {
				return lines.size() ? lines[0] : AtChannel::empty_line;
			}
			
			const inline int isCmeError() const {
				return getCmeError() >= 0;
			}
			
			const inline int isCmsError() const {
				return getCmsError() >= 0;
			}
			
			const inline int isGeneralError() const {
				return !isCmeError() && !isCmsError() && error;
			}
			
			const int getCmeError() const;
			const int getCmsError() const;
		};
		
		enum ResultType {
			DEFAULT,
			MULTILINE,
			NUMERIC,
			NO_RESPONSE,
			DIAL,
			NO_PREFIX
		};
	protected:
		struct UnsolHandler {
			std::string prefix;
			std::function<void(std::string)> handler;
		};
		
		std::vector<UnsolHandler> m_unsol_handlers;
		std::vector<std::string> m_unsol_queue;
		
		int m_fd = -1;
		Serial *m_serial = nullptr;
		bool m_verbose = false;
		bool m_stop = false;
		
		static constexpr int MAX_AT_RESPONSE = 8 * 1024;
		
		std::string m_buffer = "";
		Response *m_curr_response = nullptr;
		std::string m_curr_prefix = "";
		ResultType m_curr_type = DEFAULT;
		Semaphore m_cmd_sem;
		std::mutex at_cmd_mutex;
		TimeoutSetCallback m_timeout_callback;
		int m_default_at_timeout = 10 * 1000;
		bool m_busy = false;
		
		std::function<void()> m_broken_io_handler;
		std::function<void(Errors error, int64_t start)> m_global_error_handler;
		
		// thread
		std::thread m_thread;
		bool m_started = false;
		
		static bool isErrorResponse(const std::string &line, bool dial = false);
		static bool isSuccessResponse(const std::string &line, bool dial = false);
		
		void handleLine();
		void handleUnsolicitedLine();
	public:
		AtChannel();
		~AtChannel();
		
		inline void setSerial(Serial *serial) {
			m_serial = serial;
		}

		inline void setVerbose(bool verbose) {
			m_verbose = verbose;
		}
		
		inline void setDefaultTimeout(int timeout) {
			m_default_at_timeout = timeout;
		}
		
		inline void setDefaultTimeoutCallback(const TimeoutSetCallback &callback) {
			m_timeout_callback = callback;
		}
		
		void readerLoop();
		
		int sendCommand(ResultType type, const std::string &cmd, const std::string &prefix, Response *response, int timeout = 0);
		
		void onUnsolicited(const std::string &prefix, const std::function<void(const std::string &)> &handler);
		
		inline void onIoBroken(const std::function<void()> &handler) {
			m_broken_io_handler = handler;
		}
		
		inline void onAnyError(const std::function<void(Errors error, int64_t start)> &handler) {
			m_global_error_handler = handler;
		}
		
		void resetUnsolicitedHandlers();
		
		bool start();
		void stop();
		
		inline bool busy() {
			return m_busy;
		}
		
		inline Response sendCommand(const std::string &cmd, const std::string &prefix = "", int timeout = 0) {
			Response response;
			sendCommand(DEFAULT, cmd, prefix, &response, timeout);
			return response;
		}
		
		inline Response sendCommandNoPrefix(const std::string &cmd, int timeout = 0) {
			Response response;
			sendCommand(NO_PREFIX, cmd, "", &response, timeout);
			return response;
		}
		
		inline Response sendCommandMultiline(const std::string &cmd, const std::string &prefix = "", int timeout = 0) {
			Response response;
			sendCommand(MULTILINE, cmd, prefix, &response, timeout);
			return response;
		}
		
		inline Response sendCommandNumeric(const std::string &cmd, int timeout = 0) {
			Response response;
			sendCommand(NUMERIC, cmd, "", &response, timeout);
			return response;
		}
		
		inline Response sendCommandNumericOrWithPrefix(const std::string &cmd, const std::string &prefix = "", int timeout = 0) {
			Response response;
			sendCommand(NUMERIC, cmd, prefix, &response, timeout);
			return response;
		}
		
		inline int sendCommandNoResponse(const std::string &cmd, int timeout = 0) {
			Response response;
			return sendCommand(NO_RESPONSE, cmd, "", &response, timeout);
		}
		
		bool checkCommandExists(const std::string &cmd, int timeout = 0);
		
		inline Response sendCommandDial(const std::string &cmd, int timeout = 0) {
			Response response;
			sendCommand(DIAL, cmd, "", &response, timeout);
			return response;
		}
};
