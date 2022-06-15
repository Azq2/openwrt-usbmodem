#include "BaseAt.h"

#include <unistd.h>
#include <Core/Loop.h>

// AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?

BaseAtModem::BaseAtModem() : Modem() {
	m_at.setVerbose(true);
	m_at.setSerial(&m_serial);
	m_at.setDefaultTimeoutCallback([this](const std::string &cmd) {
		return getCommandTimeout(cmd);
	});
}

bool BaseAtModem::execAtList(const char **commands, bool break_on_fail) {
	bool success = true;
	while (*commands) {
		bool cmd_success = false;
		for (auto &cmd: strSplit("|", *commands)) {
			if (!cmd.size() || m_at.sendCommandNoResponse(cmd) == 0) {
				cmd_success = true;
				break;
			}
		}
		
		if (!cmd_success) {
			success = false;
			if (break_on_fail)
				break;
		}
		
		commands++;
	}
	return success;
}

bool BaseAtModem::ping(int tries) {
	for (int i = 0; i < tries; i++) {
		if (m_at.sendCommandNoResponse("AT") == 0)
			return true;
	}
	return false;
}

bool BaseAtModem::handshake(int tries) {
	static const char *commands[] = {"ATQ0", "ATV1", "ATE0", nullptr};
	for (int i = 0; i < tries; i++) {
		if (execAtList(commands, false))
			return true;
	}
	return false;
}

int BaseAtModem::getCommandTimeout(const std::string &cmd) {
	// For handshake
	if (cmd == "AT" || strStartsWith(cmd, "ATQ") || strStartsWith(cmd, "ATV") || strStartsWith(cmd, "ATE"))
		return 350;
	
	// Default timeout for unsolicited USSD response
	if (cmd == "+CUSD")
		return 110 * 1000;
	
	// Default timeout for operators search
	if (strStartsWith(cmd, "AT+COPS"))
		return 300 * 1000;
	
	// SMS send
	if (strStartsWith(cmd, "AT+CMGS"))
		return 90 * 1000;
	
	// SMS write
	if (strStartsWith(cmd, "AT+CMGW"))
		return 90 * 1000;
	
	// PhoneBook write
	if (strStartsWith(cmd, "AT+CPBW"))
		return 90 * 1000;
	
	// Pincode
	if (strStartsWith(cmd, "AT+CPIN"))
		return 185 * 1000;
	
	// Default timeout
	return 0;
}

bool BaseAtModem::open() {
	// Try open serial
	if (m_serial.open(m_tty, m_speed) != 0) {
		LOGE("Can't open %s with speed %d...\n", m_tty.c_str(), m_speed);
		return false;
	}
	
	// Detect TTY device lost
	m_at.onIoBroken([this]() {
		LOGE("IO broken...\n");
		Loop::setTimeout([this]() {
			emit<EvIoBroken>({});
			m_at.stop();
		}, 0);
	});
	
	// Detect modem hangs
	m_at.onAnyError([this](AtChannel::Errors error, int64_t start) {
		if (error == AtChannel::AT_IO_ERROR || error == AtChannel::AT_TIMEOUT) {
			if (!m_self_test) {
				m_self_test = true;
				LOGE("Detected IO error on AT channel... try ping for check...\n");
				if (!ping(32)) {
					LOGE("AT channel is broken!!!\n");
					emit<EvIoBroken>({});
					m_at.stop();
				}
				m_self_test = false;
			}
		}
	});
	
	m_at.start();
	
	if (!handshake()) {
		LOGE("AT handshake failed, modem not responding...\n");
		return false;
	}
	
	if (!init()) {
		LOGE("Modem initialization failed...\n");
		return false;
	}
	
	return true;
}

bool BaseAtModem::close() {
	m_at.stop();
	return true;
}

BaseAtModem::IfaceProto BaseAtModem::getIfaceProto() {
	return IFACE_STATIC;
}

int BaseAtModem::getDelayAfterDhcpRelease() {
	return 0;
}

std::pair<bool, std::string> BaseAtModem::sendAtCommand(const std::string &cmd, int timeout) {
	auto response = m_at.sendCommandNoPrefixAll(cmd, timeout);
	
	std::string out;
	
	if (response.lines.size() > 0) {
		out = strJoin("\n", response.lines) + "\n" + response.status;
	} else {
		out = response.status;
	}
	
	return std::make_pair(response.error == 0, out);
}

bool BaseAtModem::setOption(const std::string &name, const std::any &value) {
	if (name == "tty_speed") {
		m_speed = std::any_cast<int>(value);
		return true;
	} else if (name == "tty_device") {
		m_tty = std::any_cast<std::string>(value);
		return true;
	} else if (name == "iface") {
		m_iface = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pdp_type") {
		m_pdp_type = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pdp_apn") {
		m_pdp_apn = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pdp_auth_mode") {
		m_pdp_auth_mode = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pdp_user") {
		m_pdp_user = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pdp_password") {
		m_pdp_password = std::any_cast<std::string>(value);
		return true;
	} else if (name == "pincode") {
		m_pincode = std::any_cast<std::string>(value);
		return true;
	} else if (name == "connect_timeout") {
		m_connect_timeout = std::any_cast<int>(value);
		return true;
	} else if (name == "prefer_sms_to_sim") {
		m_prefer_sms_to_sim = std::any_cast<bool>(value);
		return true;
	}
	return false;
}

std::vector<BaseAtModem::Capability> BaseAtModem::getCapabilities() {
	return {};
}
