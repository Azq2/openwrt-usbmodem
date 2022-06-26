#include "../HuaweiNcm.h"
#include <Core/Loop.h>

int HuaweiNcmModem::getDelayAfterDhcpRelease() {
	return 0;
}

HuaweiNcmModem::IfaceProto HuaweiNcmModem::getIfaceProto() {
	// Modem supports two different protocols
	if (m_prefer_dhcp)
		return IFACE_DHCP;
	return IFACE_STATIC;
}

bool HuaweiNcmModem::syncApn() {
	std::string cmd;
	
	int auth_type = 0;
	if (m_pdp_auth_mode == "pap")
		auth_type = 1;
	if (m_pdp_auth_mode == "chap")
		auth_type = 2;
	
	for (int i = 0; i < 2; i++) {
		cmd = strprintf("AT+CGDCONT=%d,\"%s\",\"%s\"", i, m_pdp_type.c_str(), m_pdp_apn.c_str());
		if (m_at.sendCommandNoResponse(cmd) != 0)
			return false;
		
		if (auth_type) {
			cmd = strprintf("AT^AUTHDATA=%d,%d,\"\",\"%s\",\"%s\"", i, auth_type, m_pdp_user.c_str(), m_pdp_password.c_str());
			if (m_at.sendCommandNoResponse(cmd) != 0)
				return false;
		} else {
			cmd = strprintf("AT^AUTHDATA=%d", i);
			if (m_at.sendCommandNoResponse(cmd) != 0)
				return false;
		}
	}
	
	return true;
}

bool HuaweiNcmModem::dial() {
	std::string cmd;
	auto response = m_at.sendCommandDial("AT^NDISDUP=1,1");
	if (response.error) {
		LOGD("Dial error: %s\n", response.status.c_str());
		return false;
	}
	return true;
}

bool HuaweiNcmModem::readDhcpV4() {
	uint32_t addr, gw, mask, dns1, dns2;
	auto response = m_at.sendCommand("AT^DHCP?", "^DHCP");
	if (response.error)
		return false;
	
	bool success = AtParser(response.data())
		.parseUInt(&addr, 16)
		.parseUInt(&mask, 16)
		.parseUInt(&gw, 16)
		.parseSkip()
		.parseUInt(&dns1, 16)
		.parseUInt(&dns2, 16)
		.success();
	
	if (!success)
		return false;
	
	m_ipv4.ip = long2ip(addr);
	m_ipv4.gw = long2ip(gw);
	m_ipv4.mask = long2ip(mask);
	m_ipv4.dns1 = long2ip(dns1);
	m_ipv4.dns2 = long2ip(dns2);
	
	return true;
}

bool HuaweiNcmModem::readDhcpV6() {
	auto response = m_at.sendCommand("AT^DHCPV6?", "^DHCPV6");
	if (response.error)
		return false;
	
	bool success = AtParser(response.data())
		.parseString(&m_ipv6.ip)
		.parseString(&m_ipv6.mask)
		.parseString(&m_ipv6.gw)
		.parseSkip()
		.parseString(&m_ipv6.dns1)
		.parseString(&m_ipv6.dns2)
		.success();
	
	if (!success)
		return false;
	
	return true;
}

void HuaweiNcmModem::handleConnect() {
	auto response = m_at.sendCommand("AT^NDISSTATQRY?", "^NDISSTATQRY");
	if (response.error) {
		handleConnectError();
		return;
	}
	
	int stat_ipv4, stat_ipv6;
	
	bool success = AtParser(response.data())
		.parseInt(&stat_ipv4)
		.parseSkip()
		.parseSkip()
		.parseSkip()
		.parseInt(&stat_ipv6)
		.parseSkip()
		.parseSkip()
		.parseSkip()
		.success();
	
	if (!success) {
		handleConnectError();
		return;
	}
	
	if (stat_ipv4 == 1) {
		if (!readDhcpV4()) {
			handleConnectError();
			return;
		}
	} else {
		m_ipv4.gw = "";
		m_ipv4.ip = "";
		m_ipv4.mask = "";
		m_ipv4.dns1 = "";
		m_ipv4.dns2 = "";
	}
	
	if (stat_ipv6 == 1) {
		if (!readDhcpV6()) {
			handleConnectError();
			return;
		}
	} else {
		m_ipv6.gw = "";
		m_ipv6.ip = "";
		m_ipv6.mask = "";
		m_ipv6.dns1 = "";
		m_ipv6.dns2 = "";
	}
	
	if (stat_ipv4 == 1 || stat_ipv6 == 1) {
		bool is_update = (m_data_state == CONNECTED);
		m_data_state = CONNECTED;
		emit<EvDataConnected>({
			.is_update	= is_update,
			.ipv4		= m_ipv4,
			.ipv6		= m_ipv6,
		});
	} else {
		handleDisconnect();
	}
}

void HuaweiNcmModem::handleConnectError() {
	LOGE("Can't get PDP context info...\n");
	handleDisconnect();
}

void HuaweiNcmModem::handleDisconnect() {
	m_ipv4.gw = "";
	m_ipv4.ip = "";
	m_ipv4.mask = "";
	m_ipv4.dns1 = "";
	m_ipv4.dns2 = "";
	
	m_ipv6.gw = "";
	m_ipv6.ip = "";
	m_ipv6.mask = "";
	m_ipv6.dns1 = "";
	m_ipv6.dns2 = "";
	
	if (m_data_state == CONNECTED) {
		m_data_state = DISCONNECTED;
		emit<EvDataDisconnected>({});
	} else {
		m_data_state = DISCONNECTED;
	}
}

void HuaweiNcmModem::startDataConnection() {
	if (m_tech == TECH_NO_SERVICE || !isPacketServiceReady())
		return;
	
	// Connection already scheduled
	if (m_manual_connect_timeout != -1)
		return;
	
	// Already connected or connecting
	if (m_data_state != DISCONNECTED)
		return;
	
	m_data_state = CONNECTING;
	emit<EvDataConnecting>({});
	
	Loop::setTimeout([this]() {
		if (m_first_data_connect) {
			handleConnect();
			m_first_data_connect = false;
			
			if (m_data_state != DISCONNECTED)
				return;
		}
		
		if (dial()) {
			handleConnect();
		} else {
			handleDisconnect();
			
			// Try reconnect after few seconds
			m_manual_connect_timeout = Loop::setTimeout([this]() {
				m_manual_connect_timeout = -1;
				startDataConnection();
			}, 1000);
		}
	}, 0);
}
