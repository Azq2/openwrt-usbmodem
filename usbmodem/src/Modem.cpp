#include "Modem.h"

Modem::Modem() {
	m_at.setVerbose(true);
	m_at.setSerial(&m_serial);
	m_at.setDefaultTimeout(getDefaultAtTimeout());
}

Modem::~Modem() {
	
}

int Modem::getDefaultAtTimeout() {
	return 10 * 1000;
}

int Modem::getDefaultAtPingTimeout() {
	return 300;
}

int Modem::getDelayAfterDhcpRelease() {
	return 0;
}

Modem::IfaceProto Modem::getIfaceProto() {
	return IFACE_DHCP;
}

const char *Modem::getTechName(NetworkTech tech) {
	switch (tech) {
		case TECH_UNKNOWN:		return "unknown";
		case TECH_NO_SERVICE:	return "No service";
		case TECH_GSM:			return "GSM";
		case TECH_GPRS:			return "2G (GPRS)";
		case TECH_EDGE:			return "2G (EDGE)";
		case TECH_UMTS:			return "3G (UMTS)";
		case TECH_HSDPA:		return "3G (HSDPA)";
		case TECH_HSUPA:		return "3G (HSUPA)";
		case TECH_HSPA:			return "3G (HSPA)";
		case TECH_HSPAP:		return "3G (HSPA+)";
		case TECH_LTE:			return "4G (LTE)";
	}
	return "unknown";
}

const char *Modem::getNetRegStatusName(NetworkReg reg) {
	switch (reg) {
		case NET_NOT_REGISTERED:		return "not_registered";
		case NET_SEARCHING:				return "searching";
		case NET_REGISTERED_HOME:		return "home";
		case NET_REGISTERED_ROAMING:	return "roaming";
	}
	return "unknown";
}

bool Modem::sendUssd(std::string ussd, std::function<void(int code, const std::string &)> callback, int timeout) {
	std::string cmd = "AT+CUSD=1,\"" + ussd + "\",15";
	
	if (isUssdBusy())
		return false;
	
	m_current_ussd_callback = callback;
	
	if (m_at.sendCommandNoResponse(cmd) != 0) {
		m_current_ussd_callback = nullptr;
		return false;
	}
	
	if (!timeout)
		timeout = TIMEOUT_USSD;
	
	m_ussd_timeout = Loop::setTimeout([=]() {
		m_current_ussd_callback = nullptr;
		callback(-1, "");
	}, timeout);
	
	return true;
}

bool Modem::ping(int tries) {
	for (int i = 0; i < tries; i++) {
		if (m_at.sendCommandNoResponse("AT", getDefaultAtPingTimeout()) == 0)
			return true;
	}
	return false;
}

bool Modem::handshake() {
	m_self_test = true;
	
	bool success = true;
	for (int i = 0; i < 3; i++) {
		success = true;
		
		// enable result codes
		if (m_at.sendCommandNoResponse("ATQ0", getDefaultAtPingTimeout()) != 0)
			success = false;
		
		// enable verbose
		if (m_at.sendCommandNoResponse("ATV1", getDefaultAtPingTimeout()) != 0)
			success = false;
		
		// echo off
		if (m_at.sendCommandNoResponse("ATE0", getDefaultAtPingTimeout()) != 0)
			success = false;
		
		if (success)
			break;
	}
	
	m_self_test = false;
	
	return success;
}

void Modem::getSignalLevels(SignalLevels *levels) const {
	levels->rssi_dbm = m_rssi_dbm;
	levels->bit_err_pct = m_bit_err_pct;
	levels->rscp_dbm = m_rscp_dbm;
	levels->eclo_db = m_eclo_db;
	levels->rsrq_db = m_rsrq_db;
	levels->rsrp_dbm = m_rsrp_dbm;
}

bool Modem::getIpInfo(int ipv, IpInfo *ip_info) const {
	if (ipv == 6) {
		ip_info->ip = m_ipv6_ip;
		ip_info->mask = m_ipv6_mask;
		ip_info->gw = m_ipv6_gw;
		ip_info->dns1 = m_ipv6_dns1;
		ip_info->dns2 = m_ipv6_dns2;
		return m_ipv6_ip.size() > 0;
	} else {
		ip_info->ip = m_ipv4_ip;
		ip_info->mask = m_ipv4_mask;
		ip_info->gw = m_ipv4_gw;
		ip_info->dns1 = m_ipv4_dns1;
		ip_info->dns2 = m_ipv4_dns2;
		return m_ipv4_ip.size() > 0;
	}
}

void Modem::stopNetRegWhatchdog() {
	if (m_connect_timeout_id >= 0) {
		Loop::clearTimeout(m_connect_timeout_id);
		m_connect_timeout_id = -1;
	}
}

void Modem::startNetRegWhatchdog() {
	if (m_connect_timeout_id >= 0 || m_connect_timeout <= 0)
		return;
	
	m_connect_timeout_id = Loop::setTimeout([=]() {
		m_connect_timeout_id = -1;
		Loop::emit<EvDataConnectTimeout>({});
	}, m_connect_timeout);
}

void Modem::handleCusd(const std::string &event) {
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
	
	if (success) {
		int upper = (dcs & 0xF0) >> 4;
		int lower = dcs & 0x0F;
		
		// UCS2
		if (upper == 1 && lower == 1) {
			data = decodeUcs2(hex2bin(data), true);
		}
		// 7bit
		else {
			data = decode7bit(hex2bin(data));
		}
	} else {
		code = -1;
	}
	
	if (m_current_ussd_callback) {
		Loop::setTimeout([=]() {
			Loop::clearTimeout(m_ussd_timeout);
			
			auto callback = m_current_ussd_callback;
			
			m_current_ussd_callback = nullptr;
			m_ussd_timeout = -1;
			
			if (callback)
				callback(code, data);
		}, 0);
	}
}

void Modem::handleCpin(const std::string &event) {
	std::string code;
	if (!AtParser(event).parseString(&code).success())
		return;
	
	PinState old_state = m_pin_state;
	
	if (strStartsWith(code, "READY")) {
		m_pin_state = PIN_READY;
	} else if (strStartsWith(code, "SIM PIN")) {
		if (!m_pincode.size()) {
			LOGE("SIM pin required, but no pincode specified...\n");
			m_pin_state = PIN_ERROR;
		} else if (m_pincode_entered) {
			m_pin_state = PIN_ERROR;
		} else {
			m_pin_state = PIN_REQUIRED;
			
			// Trying enter PIN code only one time
			m_pincode_entered = true;
			
			Loop::setTimeout([=]() {
				if (m_at.sendCommandNoResponse("AT+CPIN=" + m_pincode) != 0)
					LOGE("SIM PIN unlock error\n");
				
				// Force request new status
				m_at.sendCommandNoResponse("AT+CPIN?");
			}, 0);
		}
	} else {
		LOGE("SIM required other lock code: %s\n", code.c_str());
		m_pin_state = PIN_ERROR;
	}
	
	if (old_state != m_pin_state)
		Loop::emit<EvPinStateChaned>({.state = m_pin_state});
}

void Modem::handleCesq(const std::string &event) {
	static const double bit_errors[] = {0.14, 0.28, 0.57, 1.13, 2.26, 4.53, 9.05, 18.10};
	int rssi, ber, rscp, eclo, rsrq, rsrp;
	
	bool parsed = AtParser(event)
		.parseInt(&rssi)
		.parseInt(&ber)
		.parseInt(&rscp)
		.parseInt(&eclo)
		.parseInt(&rsrq)
		.parseInt(&rsrp)
		.success();
	
	if (!parsed)
		return;
	
	// RSSI (Received signal strength)
	m_rssi_dbm = -(rssi >= 99 ? NAN : 111 - rssi);
	
	// Bit Error
	m_bit_err_pct = ber >= 0 && ber < COUNT_OF(bit_errors) ? bit_errors[ber] : NAN;
	
	// RSCP (Received signal code power)
	m_rscp_dbm = -(rscp >= 255 ? NAN : 121 - rscp);
	
	// Ec/lo
	m_eclo_db = -(eclo >= 255 ? NAN : (49.0f - (float) eclo) / 2.0f);
	
	// RSRQ (Reference signal received quality)
	m_rsrq_db = -(rsrq >= 255 ? NAN : (40.0f - (float) rsrq) / 2.0f);
	
	// RSRP (Reference signal received power)
	m_rsrp_dbm = -(rsrp >= 255 ? NAN : 141 - rsrp);
	
	Loop::emit<EvSignalLevels>({});
}

bool Modem::open() {
	// Try open serial
	if (m_serial.open(m_tty, m_speed) != 0) {
		LOGE("Can't open %s with speed %d...\n", m_tty.c_str(), m_speed);
		return false;
	}
	
	// Detect TTY device lost
	m_at.onIoBroken([=]() {
		Loop::setTimeout([=]() {
			Loop::emit<EvIoBroken>({});
			m_at.stop();
		}, 0);
	});
	
	// Detect modem hangs
	m_at.onAnyError([=](AtChannel::Errors error, int64_t start) {
		if (error == AtChannel::AT_IO_ERROR || error == AtChannel::AT_TIMEOUT) {
			if (!m_self_test) {
				m_self_test = true;
				LOGE("Detected IO error on AT channel... try ping for check...\n");
				if (!ping(32)) {
					LOGE("AT channel is broken!!!\n");
					Loop::emit<EvIoBroken>({});
					m_at.stop();
				}
				m_self_test = false;
			}
		}
	});
	
	// Start AT channel
	if (!m_at.start()) {
		LOGE("Can't start AT channel...\n");
		return false;
	}
	
	// Try handshake
	if (!handshake()) {
		LOGE("Can't handshake modem...\n");
		close();
		return false;
	}
	
	// Init modem
	if (!init()) {
		LOGE("Can't init modem...\n");
		close();
		return false;
	}
	
	return true;
}

void Modem::finish() {
	
}

void Modem::close() {
	m_at.stop();
}
