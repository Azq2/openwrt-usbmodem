#include "../BaseAt.h"
#include <Core/Loop.h>

void BaseAtModem::handleUssdResponse(int code, const std::string &data, int dcs) {
	auto [success, decoded] = decodeCbsDcsString(data, dcs);
	if (!success) {
		code = USSD_ERROR;
		decoded = "Can't decode USSD: " + bin2hex(data, true) + " [dcs=" + std::to_string(dcs) + "].";
		LOGE("%s\n", decoded.c_str());
	}
	
	if (m_ussd_callback) {
		uint32_t current_req = m_ussd_request_id;
		
		Loop::setTimeout([=, this]() {
			if (current_req == m_ussd_request_id) {
				auto callback = m_ussd_callback;
				Loop::clearTimeout(m_ussd_timeout);
				m_ussd_callback = nullptr;
				m_ussd_timeout = -1;
				m_ussd_session = (code == USSD_WAIT_REPLY);
				
				if (callback)
					callback(static_cast<UssdCode>(code), decoded);
			} else if (!isUssdBusy()) {
				LOGD("Unsolicited USSD: %s [code=%d]\n", decoded.c_str(), code);
				
				if (code == USSD_WAIT_REPLY)
					cancelUssd();
			}
		}, 0);
	}
}

void BaseAtModem::handleCusd(const std::string &event) {
	int dcs = 0, code = 0;
	std::string data;
	
	bool success;
	int args_cnt = AtParser::getArgCnt(event);
	
	if (args_cnt == 1) {
		success = AtParser(event)
			.parseInt(&code)
			.success();
	} else {
		success = AtParser(event)
			.parseInt(&code)
			.parseString(&data)
			.parseInt(&dcs)
			.success();
	}
	
	if (!success) {
		LOGE("Invalid CUSD: %s\n", event.c_str());
		return;
	}
	
	// NOTE: hex2bin assume as raw data, if can't decode as hex
	handleUssdResponse(code, hex2bin(data), dcs);
}

bool BaseAtModem::sendUssd(const std::string &cmd, UssdCallback callback, int timeout) {
	if (!timeout)
		timeout = getCommandTimeout("+CUSD");
	
	if (!isValidUssd(cmd)) {
		callback(USSD_ERROR, "Not valid ussd command.");
		return false;
	}
	
	if (isUssdBusy()) {
		if (!cancelUssd()) {
			callback(USSD_ERROR, "Previous USSD command not finished.");
			return false;
		}
	}
	
	m_ussd_request_id++;
	m_ussd_callback = callback;
	
	if (m_at.sendCommandNoResponse("AT+CUSD=1,\"" + cmd + "\",15") != 0) {
		m_ussd_callback = nullptr;
		return false;
	}
	
	m_ussd_timeout = Loop::setTimeout([this, callback]() {
		m_ussd_callback = nullptr;
		callback(USSD_ERROR, "USSD command timeout reached.");
	}, timeout);
	
	return true;
}

bool BaseAtModem::cancelUssd() {
	auto last_callback = m_ussd_callback;
	m_ussd_callback = nullptr;
	m_ussd_session = false;
	
	if (m_ussd_timeout != -1) {
		Loop::clearTimeout(m_ussd_timeout);
		m_ussd_timeout = -1;
	}
	
	if (last_callback) {
		Loop::setTimeout([last_callback]() {
			last_callback(USSD_ERROR, "USSD command canceled.");
		}, 0);
	}
	
	return m_at.sendCommandNoResponse("AT+CUSD=2") == 0;
}

bool BaseAtModem::isUssdBusy() {
	return m_ussd_callback != nullptr;
}

bool BaseAtModem::isUssdWaitReply() {
	return m_ussd_session;
}
