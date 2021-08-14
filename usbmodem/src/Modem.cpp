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

bool Modem::ping() {
	for (int i = 0; i < 3; i++) {
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
	if (m_data_connect_timeout_id >= 0) {
		Loop::clearTimeout(m_data_connect_timeout_id);
		m_data_connect_timeout_id = -1;
	}
}

void Modem::startNetRegWhatchdog() {
	if (m_data_connect_timeout_id >= 0 || m_data_connect_timeout <= 0)
		return;
	
	m_data_connect_timeout_id = Loop::setTimeout([=]() {
		m_data_connect_timeout_id = -1;
		Loop::emit<EvDataConnectTimeout>({});
	}, m_data_connect_timeout);
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
				if (!ping() && !ping() && !ping()) {
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
