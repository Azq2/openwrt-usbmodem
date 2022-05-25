#include "AtChannel.h"
#include "AtParser.h"

#include <signal.h>
#include <unistd.h>
#include <stdexcept>

const std::string AtChannel::empty_line;
			
const int AtChannel::Response::getCmeError() const {
	if (strStartsWith(status, "+CME ERROR")) {
		int error;
		if (AtParser(status).parseInt(&error).success())
			return error;
	}
	return -1;
}

const int AtChannel::Response::getCmsError() const {
	if (strStartsWith(status, "+CMS ERROR")) {
		int error;
		if (AtParser(status).parseInt(&error).success())
			return error;
	}
	return -1;
}

AtChannel::AtChannel() {
	
}

AtChannel::~AtChannel() {
	
}

bool AtChannel::start() {
	if (!m_started) {
		m_stop = false;
		m_started = true;
		
		m_thread = std::thread([this]() {
			readerLoop();
		});
	}
	return true;
}

void AtChannel::stop() {
	if (m_started) {
		m_stop = true;
		
		// Break current serial transfer
		m_serial->breakTransfer();
		
		// Wait for reader loop done
		m_thread.join();
		
		m_started = false;
	}
}

void AtChannel::onUnsolicited(const std::string &prefix, const std::function<void(const std::string &)> &handler) {
	m_unsol_handlers.push_back({.prefix = prefix + ":", .handler = handler});
}

void AtChannel::resetUnsolicitedHandlers() {
	m_unsol_handlers.clear();
}

void AtChannel::readerLoop() {
	char tmp[256];
	
	while (!m_stop) {
		int readed = m_serial->readChunk(tmp, sizeof(tmp), 30000);
		if (m_stop)
			break;
		
		// Serial device lost
		if (readed == Serial::ERR_BROKEN) {
			m_stop = true;
			
			if (m_curr_response) {
				m_curr_response->error = AT_IO_BROKEN;
				m_curr_response = nullptr;
				m_cmd_sem.post();
			}
			
			if (m_broken_io_handler)
				m_broken_io_handler();
		}
		
		if (readed < 0) {
			LOGE("Serial::readChunk error: %d\n", readed);
			continue;
		}
		
		for (int i = 0; i < readed; i++) {
			m_buffer += tmp[i];
			if (strHasEol(m_buffer)) {
				// Trim \r\n at end
				m_buffer.erase(m_buffer.size() - 2);
				
				// Hande line if not empty after trim
				if (m_buffer.size() > 0)
					handleLine();
				
				// Reset buffer
				m_buffer = "";
			}
		}
	}
}

bool AtChannel::isErrorResponse(const std::string &line, bool dial) {
	if (strStartsWith(line, "ERROR") || strStartsWith(line, "+CMS ERROR") || strStartsWith(line, "+CME ERROR"))
		return true;
	
	if (dial) {
		if (strStartsWith(line, "NO CARRIER") || strStartsWith(line, "NO ANSWER") || strStartsWith(line, "NO DIALTONE"))
			return true;
	}
	
	return false;
}

bool AtChannel::isSuccessResponse(const std::string &line, bool dial) {
	if (strStartsWith(line, "OK"))
		return true;
	
	if (dial) {
		if (strStartsWith(line, "CONNECT"))
			return true;
	}
	
	return false;
}

void AtChannel::handleUnsolicitedLine() {
	if (m_verbose)
		LOGD("AT -- %s\n", m_buffer.c_str());
	
	for (auto &h: m_unsol_handlers) {
		if (strStartsWith(m_buffer, h.prefix))
			h.handler(m_buffer);
	}
}

void AtChannel::handleLine() {
	if (m_curr_response) {
		if (isSuccessResponse(m_buffer, m_curr_type == DIAL)) {
			m_curr_response->error = AT_SUCCESS;
			m_curr_response->status = m_buffer;
			
			m_curr_response = nullptr;
			m_cmd_sem.post();
		} else if (isErrorResponse(m_buffer, m_curr_type == DIAL)) {
			m_curr_response->error = AT_ERROR;
			m_curr_response->status = m_buffer;
			
			m_curr_response = nullptr;
			m_cmd_sem.post();
			m_curr_response = nullptr;
		} else if (m_curr_type == DEFAULT) {
			if (strStartsWith(m_buffer, m_curr_prefix)) {
				m_curr_response->lines.push_back(m_buffer);
			} else {
				handleUnsolicitedLine();
			}
		} else if (m_curr_type == NO_PREFIX) {
			m_curr_response->lines.push_back(m_buffer);
			handleUnsolicitedLine();
		} else if (m_curr_type == NUMERIC) {
			if (m_curr_prefix.size() > 0 && strStartsWith(m_buffer, m_curr_prefix)) {
				m_curr_response->lines.push_back(m_buffer);
			} else if (isdigit(m_buffer[0])) {
				m_curr_response->lines.push_back(m_buffer);
			} else {
				handleUnsolicitedLine();
			}
		} else if (m_curr_type == MULTILINE) {
			if (strStartsWith(m_buffer, m_curr_prefix)) {
				m_curr_response->lines.push_back(m_buffer);
			} else if (m_curr_response->lines.size() > 0) {
				if (m_buffer[0] == '+' || m_buffer[0] == '*' || m_buffer[0] == '^') {
					handleUnsolicitedLine();
				} else {
					m_curr_response->lines.back() += "\r\n" + m_buffer;
				}
			}
		} else {
			handleUnsolicitedLine();
		}
	} else {
		handleUnsolicitedLine();
	}
}

bool AtChannel::checkCommandExists(const std::string &cmd, int timeout) {
	Response response;
	
	int ret = sendCommand(NO_RESPONSE, cmd, "", &response, timeout);
	if (ret == AT_SUCCESS)
		return true;
	
	if (strStartsWith(response.status, "+CME") || strStartsWith(response.status, "+CMS"))
		return true;
	
	return false;
}

int AtChannel::sendCommand(ResultType type, const std::string &cmd, const std::string &prefix, Response *response, int timeout) {
	if ((type == DEFAULT || type == MULTILINE) && prefix == "")
		type = NO_RESPONSE;
	
	if (m_stop) {
		LOGE("[ %s ] error, AT channel already closed...\n", cmd.c_str());
		return AT_IO_BROKEN;
	}
	
	if (!timeout) {
		timeout = m_timeout_callback ? m_timeout_callback(cmd) : 0;
		
		if (!timeout)
			timeout = m_default_at_timeout;
	}
	
	at_cmd_mutex.lock();
	
	int64_t start = getCurrentTimestamp();
	
	// Make sure response is clean
	response->error = AT_IO_ERROR;
	response->lines.clear();
	response->status.clear();
	
	m_curr_response = response;
	m_curr_prefix = prefix;
	m_curr_type = type;
	
	if (m_verbose)
		LOGD("AT >> %s\n", cmd.c_str());
	
	// Write AT command to modem
	std::string complete_cmd = cmd + "\r";
	int ret = m_serial->write(complete_cmd.c_str(), complete_cmd.size(), getNewTimeout(start, timeout));
	
	if (ret < 0 || ret != complete_cmd.size()) {
		response->error = AT_IO_ERROR;
		LOGE("[ %s ] serial io error\n", cmd.c_str());
	} else {
		// Wait for command finish
		if (!m_cmd_sem.wait(getNewTimeout(start, timeout))) {
			response->error = AT_TIMEOUT;
			uint32_t elapsed = getCurrentTimestamp() - start;
			LOGE("[ %s ] command timeout, elapsed = %u\n", cmd.c_str(), elapsed);
		}
	}
	
	if (response->error)
		LOGE("[ %s ] error = %d, status = %s\n", cmd.c_str(), response->error, response->status.c_str());
	
	if (m_verbose) {
		if (type != NO_PREFIX) {
			for (int i = 0; i < response->lines.size(); i++)
				LOGD("AT << %s\n", response->lines[i].c_str());
		}
		
		if (response->status.size() > 0)
			LOGD("AT << %s\n", response->status.c_str());
	}
	
	m_curr_response = nullptr;
	
	at_cmd_mutex.unlock();
	
	if (response->error && m_global_error_handler)
		m_global_error_handler(response->error, start);
	
	return response->error;
}
