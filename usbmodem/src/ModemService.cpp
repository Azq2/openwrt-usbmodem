#include "ModemService.h"

#include "Modem/Asr1802.h"
#include "Uci.h"

#include <vector>

ModemService::ModemService() {
	// Default options
	m_uci_options["proto"] = "";
	m_uci_options["modem_device"] = "";
	m_uci_options["modem_speed"] = "115200";
	m_uci_options["modem_type"] = "";
	m_uci_options["auth_type"] = "";
	m_uci_options["pdp_type"] = "IP";
	m_uci_options["apn"] = "internet";
	m_uci_options["username"] = "";
	m_uci_options["password"] = "";
	m_uci_options["pincode"] = "";
	m_uci_options["force_use_dhcp"] = "0";
	m_uci_options["force_network_restart"] = "0";
}

bool ModemService::validateOptions() {
	if (m_uci_options["proto"] != "usbmodem") {
		LOGE("Uunsupported protocol: %s\n", m_uci_options["proto"].c_str());
		return false;
	}
	
	if (!m_uci_options["modem_device"].size()) {
		LOGE("Please, specify `modem_device` in config.\n");
		return false;
	}
	
	if (!m_uci_options["modem_type"].size()) {
		LOGE("Please, specify `modem_device` in config.\n");
		return false;
	}
	
	if (m_uci_options["pdp_type"] != "IP" && m_uci_options["pdp_type"] != "IPV6" && m_uci_options["pdp_type"] != "IPV4V6") {
		LOGE("Invalid PDP type: %s\n", m_uci_options["pdp_type"].c_str());
		return false;
	}
	
	if (m_uci_options["auth_type"] != "" && m_uci_options["auth_type"] != "pap" && m_uci_options["auth_type"] != "chap") {
		LOGE("Invalid auth type: %s\n", m_uci_options["auth_type"].c_str());
		return false;
	}
	
	return true;
}

bool ModemService::startDhcp(const std::string &iface) {
	if (m_dhcp_inited) {
		if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(iface + "_4")) {
				LOGE("Can't send dhcp renew for '%s_4'\n", iface.c_str());
				return false;
			}
		}
		
		if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(iface + "_6")) {
				LOGE("Can't send dhcp renew for '%s_6'\n", iface.c_str());
				return false;
			}
		}
	} else {
		if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcp", iface + "_4", iface, m_firewall_zone, m_uci_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", iface.c_str());
				return false;
			}
		}
		
		if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcpv6", iface + "_4", iface, m_firewall_zone, m_uci_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", iface.c_str());
				return false;
			}
		}
		
		m_dhcp_inited = true;
	}
	return true;
}

bool ModemService::stopDhcp(const std::string &iface) {
	if (!m_dhcp_inited)
		return true;
	
	if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(iface + "_4")) {
			LOGE("Can't send dhcp release for '%s_4'\n", iface.c_str());
			return false;
		}
	}
	
	if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(iface + "_6")) {
			LOGE("Can't send dhcp release for '%s_6'\n", iface.c_str());
			return false;
		}
	}
	
	return true;
}

int ModemService::run(const std::string &iface) {
	m_start_time = getCurrentTimestamp();
	
	if (!Loop::init()) {
		LOGE("Can't init eventloop...\n");
		return -1;
	}
	
	if (!m_ubus.open()) {
		LOGE("Can't init ubus...\n");
		return -1;
	}
	
	m_netifd.setUbus(&m_ubus);
	
	if (!Uci::loadIfaceConfig(iface, &m_uci_options)) {
		LOGE("Can't read config for interface: %s\n", iface.c_str());
		return -1;
	}
	
	if (!Uci::loadIfaceFwZone(iface, &m_firewall_zone)) {
		LOGE("Can't find fw3 zone for interface: %s\n", iface.c_str());
		return -1;
	}
	
	if (!validateOptions())
		return -1;
	
	int tty_speed = strToInt(m_uci_options["modem_speed"]);
	std::string tty_path = findTTY(m_uci_options["modem_device"]);
	std::string net_iface = findNetByTTY(tty_path);
	
	if (!tty_path.size()) {
		LOGE("Device not found: %s\n", tty_path.c_str());
		return -1;
	}
	
	if (!net_iface.size()) {
		LOGE("Network device not found: %s\n", tty_path.c_str());
		return -1;
	}
	
	if (!m_netifd.updateIface(iface, net_iface, nullptr, nullptr)) {
		LOGE("Can't init iface...\n");
		return -1;
	}
	
	if (m_uci_options["modem_type"] == "asr1802") {
		m_modem = new ModemAsr1802();
	} else {
		LOGE("Unsupported modem type: %s\n", m_uci_options["modem_type"].c_str());
		return -1;
	}
	
	m_modem->setNetIface(net_iface);
	m_modem->setPreferDhcp(m_uci_options["force_use_dhcp"] == "1");
	m_modem->setForceNetworkRestart(m_uci_options["force_network_restart"] == "1");
	
	m_modem->setPdpConfig(m_uci_options["pdp_type"], m_uci_options["apn"], m_uci_options["auth_type"], m_uci_options["username"], m_uci_options["password"]);
	m_modem->setPinCode(m_uci_options["pincode"]);
	m_modem->setSerial(tty_path, tty_speed);
	
	Loop::on<Modem::EvNetworkChanged>([=](const auto &event) {
		if (event.status == Modem::NET_NOT_REGISTERED) {
			LOGD("Unregistered from network\n");
		} else if (event.status == Modem::NET_SEARCHING) {
			LOGD("Searching network...\n");
		} else {
			LOGD("Registered to %s network\n", Modem::getNetRegStatusName(event.status));
		}
	});
	
	Loop::on<Modem::EvTechChanged>([=](const auto &event) {
		if (event.tech == Modem::TECH_NO_SERVICE || event.tech == Modem::TECH_UNKNOWN) {
			LOGD("Network mode: none\n");
		} else {
			LOGD("Network mode: %s\n", Modem::getTechName(event.tech));
		}
	});
	
	Loop::on<Modem::EvDataConnected>([=](const auto &event) {
		int dhcp_delay = 0;
		if (event.is_update) {
			int diff = getCurrentTimestamp() - m_last_connected;
			m_last_connected = getCurrentTimestamp();
			dhcp_delay = m_modem->getDelayAfterDhcpRelease();
			LOGD("Internet connection changed, last session %d ms\n", diff);
		} else {
			m_last_connected = getCurrentTimestamp();
			if (m_last_disconnected) {
				int diff = m_last_connected - m_last_disconnected;
				dhcp_delay = std::max(0, m_modem->getDelayAfterDhcpRelease() - diff);
				LOGD("Internet connected, downtime %d ms\n", diff);
			} else {
				int diff = m_last_connected - m_start_time;
				LOGD("Internet connected, init time %d ms\n", diff);
			}
		}
		
		IpInfo ipv4 = {}, ipv6 = {};
		m_modem->getIpInfo(4, &ipv4);
		m_modem->getIpInfo(6, &ipv6);
		
		if (m_modem->getIfaceProto() == Modem::IFACE_STATIC) {
			if (!m_netifd.updateIface(iface, net_iface, &ipv4, &ipv6))
				LOGE("Can't set IP to interface '%s'\n", iface.c_str());
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			if (dhcp_delay > 0) {
				LOGD("Wait %d ms for DHCP recovery...\n", dhcp_delay);
				Loop::setTimeout([=]() {
					startDhcp(iface);
				}, dhcp_delay);
			} else {
				startDhcp(iface);
			}
		}
		
		if (ipv4.ip.size() > 0) {
			LOGD(
				"-> IPv4: ip=%s, gw=%s, mask=%s, dns1=%s, dns2=%s\n",
				ipv4.ip.c_str(), ipv4.gw.c_str(), ipv4.mask.c_str(), ipv4.dns1.c_str(), ipv4.dns2.c_str()
			);
		}
		
		if (ipv6.ip.size() > 0) {
			LOGD(
				"-> IPv6: ip=%s, gw=%s, mask=%s, dns1=%s, dns2=%s\n",
				ipv6.ip.c_str(), ipv6.gw.c_str(), ipv6.mask.c_str(), ipv6.dns1.c_str(), ipv6.dns2.c_str()
			);
		}
	});
	
	Loop::on<Modem::EvDataConnecting>([=](const auto &event) {
		LOGD("Connecting to internet...\n");
	});
	
	Loop::on<Modem::EvDataDisconnected>([=](const auto &event) {
		m_last_disconnected = getCurrentTimestamp();
		int diff = m_last_disconnected - m_last_connected;
		LOGD("Internet disconnected, last session %d ms\n", diff);
		
		if (m_modem->getIfaceProto() == Modem::IFACE_STATIC) {
			if (!m_netifd.updateIface(iface, net_iface, nullptr, nullptr))
				LOGE("Can't set IP to interface '%s'\n", iface.c_str());
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			stopDhcp(iface);
		}
	});
	
	Loop::on<Modem::EvSignalLevels>([=](const auto &event) {
		Modem::SignalLevels levels;
		m_modem->getSignalLevels(&levels);
		
		std::vector<std::string> info;
		
		if (!std::isnan(levels.rssi_dbm))
			info.push_back("RSSI: " + numberFormat(levels.rssi_dbm, 1) + " dBm");
		
		if (!std::isnan(levels.bit_err_pct))
			info.push_back("Bit errors: " + numberFormat(levels.bit_err_pct, 1) + "%");
		
		if (!std::isnan(levels.rscp_dbm))
			info.push_back("RSCP: " + numberFormat(levels.rscp_dbm, 1) + " dBm");
		
		if (!std::isnan(levels.eclo_db))
			info.push_back("Ec/lo: " + numberFormat(levels.eclo_db, 1) + " dB");
		
		if (!std::isnan(levels.rsrq_db))
			info.push_back("RSRQ: " + numberFormat(levels.rsrq_db, 1) + " dB");
		
		if (!std::isnan(levels.rsrp_dbm))
			info.push_back("RSRP: " + numberFormat(levels.rsrp_dbm, 1) + " dBm");
		
		if (info.size() > 0) {
			std::string str = strJoin(info, ", ");
			LOGD("%s\n", str.c_str());
		}
	});
	
	Loop::on<Modem::EvPinStateChaned>([=](const auto &event) {
		if (event.state == Modem::PIN_ERROR) {
			LOGD("PIN: invalid code, or required PIN2, PUK, PUK2 or other.\n");
		} else if (event.state == Modem::PIN_READY) {
			LOGD("PIN: success\n");
		} else if (event.state == Modem::PIN_REQUIRED) {
			LOGD("PIN: need unlock\n");
		}
	});
	
	if (!m_modem->open()) {
		LOGE("Can't initialize modem.\n");
		delete m_modem;
		return -1;
	}
	
	Loop::run();
	
	auto start = getCurrentTimestamp();
	Loop::done();
	LOGD("loop done\n");
	
	LOGD("finish modem\n");
	m_modem->finish();
	
	LOGD("close modem\n");
	m_modem->close();
	
	LOGD("delete modem\n");
	delete m_modem;
	auto end = getCurrentTimestamp();
	
	int diff = end - start;
	LOGD("finish time: %d ms\n", diff);
	
	return 0;
}
