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

bool Modem::handshake() {
	for (int i = 0; i < 3; i++) {
		bool success = false;
		
		// enable result codes
		if (m_at.sendCommandNoResponse("ATQ0", 250) == AtChannel::AT_SUCCESS)
			success = true;
		
		// enable verbose
		if (m_at.sendCommandNoResponse("ATV1", 250) == AtChannel::AT_SUCCESS)
			success = true;
		
		// echo off
		if (m_at.sendCommandNoResponse("ATE0", 250) == AtChannel::AT_SUCCESS)
			success = true;
		
		if (success)
			return true;
	}
	
	return false;
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

bool Modem::open() {
	// Try open serial
	if (m_serial.open(m_tty, m_speed) != 0) {
		LOGE("Can't open %s with speed %d...\n", m_tty.c_str(), m_speed);
		return false;
	}
	
	// Run AT channel
	m_at_thread = new std::thread([&]{
		m_at.readerLoop();
	});
	
	// Try handshake
	if (!handshake()) {
		LOGE("Can't handshake modem...\n");
		close();
		return false;
	}
	
	if (!init()) {
		LOGE("Can't init modem...\n");
		close();
		return false;
	}
	
	return true;
}

void Modem::close() {
	if (m_at_thread) {
		m_at.stop();
		m_at_thread->join();
		m_at_thread = nullptr;
	}
}
