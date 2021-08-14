#include "Modem.h"

Modem::Modem() {
	m_serial.setIgnoreInterrupts(true);
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

void *Modem::readerThread(void *arg) {
	Modem *self = static_cast<Modem *>(arg);
	self->m_at.readerLoop();
	return nullptr;
}

bool Modem::open() {
	// Try open serial
	if (m_serial.open(m_tty, m_speed) != 0) {
		LOGE("Can't open %s with speed %d...\n", m_tty.c_str(), m_speed);
		return false;
	}
	
	m_at.onIoBroken([=]() {
		Loop::emit<EvIoBroken>({});
	});
	
	// Run AT channel
	if (pthread_create(&m_at_thread, nullptr, readerThread, this) != 0) {
		LOGD("Can't create readerloop thread, errno=%d\n", errno);
		return false;
	}
	
	m_at_thread_created = true;
	
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

void Modem::finish() {
	
}

void Modem::close() {
	if (m_at_thread_created) {
		m_at_thread_created = false;
		
		// Allow IO interruption in reader loop
		m_serial.setIgnoreInterrupts(false);
		
		// Send stop to reader loop
		m_at.stop();
		
		// Interrupt IO in reader loop
		pthread_kill(m_at_thread, SIGINT);
		pthread_kill(m_at_thread, SIGINT);
		pthread_kill(m_at_thread, SIGINT);
		
		// Wait for reader loop done
		pthread_join(m_at_thread, nullptr);
	}
}
