#include "../Asr1802.h"
#include <Core/Loop.h>

int Asr1802Modem::getDelayAfterDhcpRelease() {
	return 2000;
}

Asr1802Modem::IfaceProto Asr1802Modem::getIfaceProto() {
	// Modem supports two different protocols
	if (m_prefer_dhcp)
		return IFACE_DHCP;
	return IFACE_STATIC;
}

int Asr1802Modem::getCurrentPdpCid() {
	auto response = m_at.sendCommand("AT+CGCONTRDP=?", "+CGCONTRDP");
	if (response.error)
		return -1;
	
	if (!response.data().size())
		return 0;
	
	int cid;
	if (!AtParser(response.data()).parseInt(&cid).success())
		return -1;
	
	return cid;
}

bool Asr1802Modem::syncApn() {
	AtParser parser;
	std::string cmd, old_pdp_type, old_pdp_apn, old_user, old_password;
	int old_etif = 0, arg_cnt, old_auth_type;
	AtChannel::Response response;
	
	int auth_type = 0;
	if (m_pdp_auth_mode == "pap")
		auth_type = 1;
	if (m_pdp_auth_mode == "chap")
		auth_type = 2;
	
	// Get LTE pdp config
	response = m_at.sendCommand("AT*CGDFLT=1", "*CGDFLT");
	if (response.error)
		return false;
	
	arg_cnt = AtParser::getArgCnt(response.data());
	
	// <PDP_type>
	parser.parse(response.data()).parseString(&old_pdp_type);
	
	// <APN>
	if (arg_cnt >= 2)
		parser.parseString(&old_pdp_apn);
	
	// <etif> (???)
	if (arg_cnt >= 20) {
		parser
			.parseSkip().parseSkip().parseSkip().parseSkip()
			.parseSkip().parseSkip().parseSkip().parseSkip()
			.parseSkip().parseSkip().parseSkip().parseSkip()
			.parseSkip().parseSkip().parseSkip().parseSkip()
			.parseSkip().parseInt(&old_etif);
	}
	
	if (!parser.success())
		return false;
	
	// Get LTE pdp auth
	response = m_at.sendCommand("AT*CGDFAUTH?", "*CGDFAUTH");
	if (response.error)
		return false;
	
	// <auth_type> <username> <password>
	parser
		.parse(response.data())
		.parseInt(&old_auth_type)
		.parseString(&old_user)
		.parseString(&old_password);
	
	if (!parser.success())
		return false;
	
	bool need_change = (
		m_pdp_type != old_pdp_type || m_pdp_apn != old_pdp_apn || old_etif != 1 ||
		m_pdp_user != old_user || m_pdp_password != old_password || auth_type != old_auth_type
	);
	
	if (need_change) {
		bool radio_on = isRadioOn();
		
		if (radio_on && !setRadioOn(false))
			return false;
		
		cmd = "AT*CGDFLT=1,\"" + m_pdp_type + "\",\"" + m_pdp_apn + "\",,,,,,,,,,,,,,,,,,1";
		if (m_at.sendCommandNoResponse(cmd) != 0)
			return false;
		
		cmd = "AT*CGDFAUTH=1," + std::to_string(auth_type) + ",\"" + m_pdp_user + "\",\"" + m_pdp_password + "\"";
		if (m_at.sendCommandNoResponse(cmd) != 0)
			return false;
		
		if (radio_on && !setRadioOn(true))
			return false;
	}
	
	return true;
}

bool Asr1802Modem::dial() {
	std::string cmd;
	
	int auth_type = 0;
	if (m_pdp_auth_mode == "pap")
		auth_type = 1;
	if (m_pdp_auth_mode == "chap")
		auth_type = 2;
	
	// Configure PDP context
	cmd = "AT+CGDCONT=" + std::to_string(m_pdp_context) + ",\"" + m_pdp_type + "\",\"" + m_pdp_apn + "\"";
	if (m_at.sendCommandNoResponse(cmd) != 0)
		return false;
	
	// Set PPP auth
	cmd = "AT*AUTHReq=" + std::to_string(m_pdp_context) + "," + std::to_string(auth_type) + ",\"" + m_pdp_user + "\",\"" + m_pdp_password + "\"";
	if (m_at.sendCommandNoResponse(cmd) != 0)
		return false;
	
	// Start dialing
	cmd = "AT+CGDATA=\"\"," + std::to_string(m_pdp_context);
	auto response = m_at.sendCommandDial(cmd);
	if (response.error) {
		LOGD("Dial error: %s\n", response.status.c_str());
		return false;
	}
	
	return true;
}

void Asr1802Modem::handleConnect() {
	std::string addr, gw, mask, dns1, dns2;
	
	// Without this command not works...
	if (m_at.sendCommandNoResponse("AT+CGDCONT?") != 0) {
		handleConnectError();
		return;
	}
	
	// Get current PDP context id
	int cid = getCurrentPdpCid();
	if (cid < 0) {
		handleConnectError();
		return;
	}
	
	// Get PDP context info
	auto response = m_at.sendCommand("AT+CGCONTRDP=" + std::to_string(cid), "+CGCONTRDP");
	if (response.error || !response.lines.size()) {
		handleConnectError();
		return;
	}
	
	for (auto &line: response.lines) {
		int arg_cnt = AtParser::getArgCnt(line);
		
		AtParser parser(line);
		
		// <cid>, <bearer_id>, <apn>
		parser.parseSkip().parseSkip().parseSkip();
		
		// <local_addr>
		if (arg_cnt >= 4)
			parser.parseString(&addr);
		
		// <subnet_mask>
		if (arg_cnt >= 5)
			parser.parseString(&mask);
		
		// <gw_addr>
		if (arg_cnt >= 6)
			parser.parseString(&gw);
		
		// <DNS_prim_addr>
		if (arg_cnt >= 7)
			parser.parseString(&dns1);
		
		// <DNS_prim_addr>
		if (arg_cnt >= 8)
			parser.parseString(&dns2);
		
		if (!parser.success()) {
			handleConnectError();
			return;
		}
		
		int ipv = getIpType(addr, true);
		if (!ipv || !normalizeIp(&addr, ipv, true)) {
			LOGE("Invalid local IP: %s\n", addr.c_str());
			handleConnectError();
			return;
		}
		
		if (mask.size() > 0 && !normalizeIp(&mask, ipv, true)) {
			LOGE("Invalid subnet mask IP: %s\n", mask.c_str());
			handleConnectError();
			return;
		}
		
		if (gw.size() > 0 && !normalizeIp(&gw, ipv, true)) {
			LOGE("Invalid gw IP: %s\n", gw.c_str());
			handleConnectError();
			return;
		}
		
		if (dns1.size() > 0 && !normalizeIp(&dns1, ipv, true)) {
			LOGE("Invalid dns1 IP: %s\n", dns1.c_str());
			handleConnectError();
			return;
		}
		
		if (dns2.size() > 0 && !normalizeIp(&dns2, ipv, true)) {
			LOGE("Invalid dns2 IP: %s\n", dns2.c_str());
			handleConnectError();
			return;
		}
		
		if (ipv == 6) {
			m_ipv6.ip = addr;
			m_ipv6.gw = gw;
			m_ipv6.mask = mask;
			m_ipv6.dns1 = dns1;
			m_ipv6.dns2 = dns2;
		} else {
			m_ipv4.ip = addr;
			m_ipv4.gw = gw;
			m_ipv4.mask = mask;
			m_ipv4.dns1 = dns1;
			m_ipv4.dns2 = dns2;
		}
	}
	
	bool is_update = (m_data_state == CONNECTED);
	
	m_data_state = CONNECTED;
	m_connect_errors = 0;
	
	emit<EvDataConnected>({
		.is_update	= is_update,
		.ipv4		= m_ipv4,
		.ipv6		= m_ipv6,
	});
}

void Asr1802Modem::handleConnectError() {
	LOGE("Can't get PDP context info...\n");
	handleDisconnect();
	restartNetwork();
}

void Asr1802Modem::handleDisconnect() {
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

void Asr1802Modem::startDataConnection() {
	// Manual connection to internet needed only for 3G/EDGE
	// On LTE modem connects to internet autmatically
	// And also we don't try connect if no service
	if (m_tech == TECH_LTE || m_tech == TECH_NO_SERVICE || m_tech == TECH_UNKNOWN || !isPacketServiceReady())
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
		if (dial()) {
			handleConnect();
		} else {
			m_connect_errors++;
			handleDisconnect();
			
			if (m_connect_errors > 10) {
				m_connect_errors = 0;
				
				LOGD("Trying restart modem network by entering to flight mode...\n");
				restartNetwork();
			}
			
			// Try reconnect after few seconds
			m_manual_connect_timeout = Loop::setTimeout([this]() {
				m_manual_connect_timeout = -1;
				startDataConnection();
			}, 1000);
		}
	}, 0);
}

void Asr1802Modem::restartNetwork() {
	setRadioOn(false);
	setRadioOn(true);
}
