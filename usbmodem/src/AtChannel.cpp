#include "AtChannel.h"

const std::string AtChannel::empty_line;

AtChannel::AtChannel() {
	if (sem_init(&at_cmd_sem, 0, 0) != 0)
		throw std::string("Can't init semaphore, fatal error...");
}

AtChannel::~AtChannel() {
	
}

void AtChannel::stop() {
	m_stop = true;
}

void AtChannel::onUnsolicited(const std::string &prefix, const std::function<void(const std::string &)> &handler) {
	m_unsol_handlers.push_back({.prefix = prefix, .handler = handler});
}

void AtChannel::readerLoop() {
	char tmp[256];
	
	m_stop = false;
	
	while (!m_stop) {
		int readed = m_serial->readChunk(tmp, sizeof(tmp), 1000);
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
		
		if (m_stop)
			LOGD("Stopping reader loop...\n");
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
			
			if (sem_post(&at_cmd_sem) != 0)
				LOGE("sem_post error: %d\n", errno);
		} else if (isErrorResponse(m_buffer, m_curr_type == DIAL)) {
			m_curr_response->error = AT_ERROR;
			m_curr_response->status = m_buffer;
			
			if (sem_post(&at_cmd_sem) != 0)
				LOGE("sem_post error: %d\n", errno);
		} else if (m_curr_type == DEFAULT) {
			if (strStartsWith(m_buffer, m_curr_prefix)) {
				m_curr_response->lines.push_back(m_buffer);
			} else {
				handleUnsolicitedLine();
			}
		} else if (m_curr_type == NUMERIC) {
			if (isdigit(m_buffer[0])) {
				m_curr_response->lines.push_back(m_buffer);
			} else {
				handleUnsolicitedLine();
			}
		} else if (m_curr_type == MULTILINE) {
			if (m_curr_response->lines.size() > 0 || strStartsWith(m_buffer, m_curr_prefix)) {
				if (m_curr_response->lines.size() > 0) {
					m_curr_response->lines[0] += "\r\n" + m_buffer;
				} else {
					m_curr_response->lines.push_back(m_buffer);
				}
			}
		} else {
			handleUnsolicitedLine();
		}
	} else {
		handleUnsolicitedLine();
	}
}

int AtChannel::sendCommand(ResultType type, const std::string &cmd, const std::string &prefix, Response *response, int timeout) {
	at_cmd_mutex.lock();
	
	if (!timeout)
		timeout = m_default_at_timeout;
	
	int64_t start = getCurrentTimestamp();
	
	// Make sure response is clean
	response->error = AT_ERROR;
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
		LOGE("[%s] serial io error\n", cmd.c_str());
	} else {
		// Wait for command finish
		struct timespec tm;
		do {
			setTimespecTimeout(&tm, getNewTimeout(start, timeout));
			ret = sem_timedwait(&at_cmd_sem, &tm);
		} while (ret == -1 && errno == EINTR);
		
		// Command timeout
		if (ret == -1) {
			if (errno == ETIMEDOUT) {
				response->error = AT_TIMEOUT;
				uint32_t elapsed = getCurrentTimestamp() - start;
				LOGE("[%s] command timeout, elapsed = %u\n", cmd.c_str(), elapsed);
			} else {
				LOGE("[%s] sem_timedwait = %d\n", cmd.c_str(), errno);
			}
		}
	}
	
	if (m_verbose) {
		for (int i = 0; i < response->lines.size(); i++)
			LOGD("AT << %s\n", response->lines[i].c_str());
		LOGD("AT << %s\n", response->status.c_str());
	}
	
	m_curr_response = nullptr;
	
	at_cmd_mutex.unlock();
	
	return response->error;
}
