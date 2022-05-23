#include "ModemService.h"

#include <vector>
#include <signal.h>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

#include "Modem/Asr1802.h"

ModemService::ModemService(const std::string &iface): m_iface(iface) {
	m_start_time = getCurrentTimestamp();
	
	m_api = new ModemServiceApi(this);
	
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
	m_uci_options["prefer_dhcp"] = "0";
	m_uci_options["prefer_sms_to_sim"] = "0";
	m_uci_options["force_network_restart"] = "0";
	m_uci_options["connect_timeout"] = "300";
	m_uci_options["preferred_sms_storage"] = "modem";
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
		LOGE("Please, specify `modem_type` in config.\n");
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

bool ModemService::startDhcp() {
	if (m_dhcp_inited) {
		if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_4")) {
				LOGE("Can't send dhcp renew for '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_6")) {
				LOGE("Can't send dhcp renew for '%s_6'\n", m_iface.c_str());
				return false;
			}
		}
	} else {
		if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcp", m_iface + "_4", m_iface, m_firewall_zone, m_uci_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcpv6", m_iface + "_4", m_iface, m_firewall_zone, m_uci_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		m_dhcp_inited = true;
	}
	return true;
}

bool ModemService::stopDhcp() {
	if (!m_dhcp_inited)
		return true;
	
	if (m_uci_options["pdp_type"] == "IP" || m_uci_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_4")) {
			LOGE("Can't send dhcp release for '%s_4'\n", m_iface.c_str());
			return false;
		}
	}
	
	if (m_uci_options["pdp_type"] == "IPV6" || m_uci_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_6")) {
			LOGE("Can't send dhcp release for '%s_6'\n", m_iface.c_str());
			return false;
		}
	}
	
	return true;
}

bool ModemService::init() {
	if (!m_ubus.open()) {
		LOGE("Can't init ubus...\n");
		return setError("INTERNAL_ERROR", true);
	}
	
	m_netifd.setUbus(&m_ubus);
	
	if (!Uci::loadIfaceConfig(m_iface, &m_uci_options)) {
		LOGE("Can't read config for interface: %s\n", m_iface.c_str());
		return setError("INVALID_CONFIG", true);
	}
	
	if (!validateOptions())
		return setError("INVALID_CONFIG", true);
	
	if (!Uci::loadIfaceFwZone(m_iface, &m_firewall_zone)) {
		LOGE("Can't find fw3 zone for interface: %s\n", m_iface.c_str());
		return setError("INVALID_CONFIG", true);
	}
	
	m_tty_speed = strToInt(m_uci_options["modem_speed"]);
	m_tty_path = findTTY(m_uci_options["modem_device"]);
	m_net_iface = findNetByTTY(m_tty_path);
	
	if (!m_tty_path.size()) {
		LOGE("Device not found: %s\n", m_uci_options["modem_device"].c_str());
		return setError("NO_DEVICE");
	}
	
	if (hasNetDev()) {
		if (!m_net_iface.size()) {
			LOGE("Network device not found: %s\n", m_uci_options["modem_device"].c_str());
			return setError("NO_DEVICE");
		}
	}
	
	// Link modem net dev to interface
	if (!m_netifd.updateIface(m_iface, m_net_iface, nullptr, nullptr)) {
		LOGE("Can't init iface...\n");
		return setError("INTERNAL_ERROR");
	}
	
	return true;
}

bool ModemService::runModem() {
	// Get modem driver
	if (m_uci_options["modem_type"] == "asr1802") {
		m_modem = new Asr1802Modem();
	} else {
		LOGE("Unsupported modem type: %s\n", m_uci_options["modem_type"].c_str());
		return setError("INVALID_CONFIG", true);
	}
	
	// Device config
	m_modem->setOption<std::string>("tty_device", m_tty_path);
	m_modem->setOption<int>("tty_speed", m_tty_speed);
	
	// PDP config
	m_modem->setOption<std::string>("pdp_type", m_uci_options["pdp_type"]);
	m_modem->setOption<std::string>("pdp_apn", m_uci_options["apn"]);
	m_modem->setOption<std::string>("pdp_auth_mode", m_uci_options["auth_type"]);
	m_modem->setOption<std::string>("pdp_user", m_uci_options["username"]);
	m_modem->setOption<std::string>("pdp_password", m_uci_options["password"]);
	
	// Security codes
	m_modem->setOption<std::string>("pincode", m_uci_options["pincode"]);
	
	// Other settings
	m_modem->setOption<bool>("prefer_dhcp", m_uci_options["prefer_dhcp"] == "1");
	m_modem->setOption<bool>("prefer_sms_to_sim", m_uci_options["prefer_sms_to_sim"] == "1");
	m_modem->setOption<int>("connect_timeout", strToInt(m_uci_options["connect_timeout"]) * 1000);
	
	/*
	m_modem->on<Modem::EvOperatorChanged>([this](const auto &event) {
		Modem::Operator op = m_modem->getOperator();
		if (op.reg != Modem::OPERATOR_REG_NONE)
			LOGD("Operator: %s - %s %s\n", op.id.c_str(), op.name.c_str(), Modem::getTechName(op.tech));
	});
	*/
	
	m_modem->on<Modem::EvNetworkChanged>([this](const auto &event) {
		LOGD("[network] %s\n", Modem::getEnumName(event.status, true));
	});
	
	m_modem->on<Modem::EvTechChanged>([this](const auto &event) {
		LOGD("[network] Tech: %s\n", Modem::getEnumName(event.tech, true));
	});
	
	m_modem->on<Modem::EvDataConnected>([this](const auto &event) {
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
		
		if (m_modem->getIfaceProto() == Modem::IFACE_STATIC) {
			if (!m_netifd.updateIface(m_iface, m_net_iface, &event.ipv4, &event.ipv6)) {
				LOGE("Can't set IP to interface '%s'\n", m_iface.c_str());
				setError("INTERNAL_ERROR");
			}
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			if (dhcp_delay > 0) {
				LOGD("Wait %d ms for DHCP recovery...\n", dhcp_delay);
				Loop::setTimeout([this]() {
					if (!startDhcp())
						setError("INTERNAL_ERROR");
				}, dhcp_delay);
			} else {
				if (!startDhcp())
					setError("INTERNAL_ERROR");
			}
		}
		
		if (event.ipv4.ip.size() > 0) {
			LOGD(
				"-> IPv4: ip=%s, gw=%s, mask=%s, dns1=%s, dns2=%s\n",
				event.ipv4.ip.c_str(), event.ipv4.gw.c_str(), event.ipv4.mask.c_str(),
				event.ipv4.dns1.c_str(), event.ipv4.dns2.c_str()
			);
		}
		
		if (event.ipv6.ip.size() > 0) {
			LOGD(
				"-> IPv6: ip=%s, gw=%s, mask=%s, dns1=%s, dns2=%s\n",
				event.ipv6.ip.c_str(), event.ipv6.gw.c_str(), event.ipv6.mask.c_str(),
				event.ipv6.dns1.c_str(), event.ipv6.dns2.c_str()
			);
		}
	});
	
	m_modem->on<Modem::EvDataConnecting>([this](const auto &event) {
		LOGD("Connecting to internet...\n");
	});
	
	m_modem->on<Modem::EvDataDisconnected>([this](const auto &event) {
		m_last_disconnected = getCurrentTimestamp();
		int diff = m_last_disconnected - m_last_connected;
		LOGD("Internet disconnected, last session %d ms\n", diff);
		
		if (m_modem->getIfaceProto() == Modem::IFACE_STATIC) {
			if (!m_netifd.updateIface(m_iface, m_net_iface, nullptr, nullptr)) {
				LOGE("Can't set IP to interface '%s'\n", m_iface.c_str());
				setError("INTERNAL_ERROR");
			}
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			if (!stopDhcp()) {
				setError("INTERNAL_ERROR");
			}
		}
	});
	
	m_modem->on<Modem::EvNetworkSignalChanged>([this](const auto &event) {
		std::vector<std::string> info;
		
		if (!std::isnan(event.signal.rssi_dbm))
			info.push_back("RSSI: " + numberFormat(event.signal.rssi_dbm, 1) + " dBm");
		
		if (!std::isnan(event.signal.bit_err_pct))
			info.push_back("Bit errors: " + numberFormat(event.signal.bit_err_pct, 1) + "%");
		
		if (!std::isnan(event.signal.rscp_dbm))
			info.push_back("RSCP: " + numberFormat(event.signal.rscp_dbm, 1) + " dBm");
		
		if (!std::isnan(event.signal.ecio_db))
			info.push_back("Ec/lo: " + numberFormat(event.signal.ecio_db, 1) + " dB");
		
		if (!std::isnan(event.signal.rsrq_db))
			info.push_back("RSRQ: " + numberFormat(event.signal.rsrq_db, 1) + " dB");
		
		if (!std::isnan(event.signal.rsrp_dbm))
			info.push_back("RSRP: " + numberFormat(event.signal.rsrp_dbm, 1) + " dBm");
		
		if (info.size() > 0) {
			std::string str = strJoin(info, ", ");
			LOGD("[network] %s\n", str.c_str());
		}
	});
	
	m_modem->on<Modem::EvSimStateChaned>([this](const auto &event) {
		LOGD("[sim] %s\n", Modem::getEnumName(event.state, true));
	});
	
	m_modem->on<Modem::EvIoBroken>([this](const auto &event) {
		LOGE("TTY device is lost...\n");
		setError("IO_ERROR");
	});
	
	m_modem->on<Modem::EvDataConnectTimeout>([this](const auto &event) {
		LOGE("Internet connection timeout...\n");
	});
	
	m_modem->on<Modem::EvSmsReady>([this](const auto &event) {
		LOGD("[sms] SMS subsystem ready!\n");
		
		Loop::setTimeout([this]() {
			auto capacity = m_modem->getSmsCapacity();
			m_sms.setMaxCapacity(capacity.total);
			
			if (!m_modem->loadSmsToDb(&m_sms, false))
				LOGE("[sms] Failed to preload exists messages from sim/modem.\n");
		}, 0);
	});
	
	if (!m_modem->open()) {
		LOGE("Can't initialize modem.\n");
		return setError("INIT_ERROR");
	}
	
	return true;
}

void ModemService::finishModem() {
	if (m_modem) {
		m_modem->close();
		m_modem = nullptr;
		delete m_modem;
	}
}

bool ModemService::setError(const std::string &code, bool fatal) {
	m_error_code = code;
	m_error_fatal = fatal;
	
	Loop::instance()->stop();
	UbusLoop::instance()->stop();
	
	return false;
}

int ModemService::checkError() {
	if (!m_error_code.size())
		return 0;
	
	// User callback
	execFile("/etc/usbmodem.user", {}, {
		"action=error",
		"error=" + m_error_code,
		"is_fatal_error=" + std::string(m_error_fatal ? "1" : "0"),
		"interface=" + m_iface
	});
	
	if (!m_netifd.avail()) {
		LOGD("%s: %s\n", (m_error_fatal ? "Fatal" : "Error"), m_error_code.c_str());
		LOGD("Can't send error to netifd, because it not inited...\n");
		sleep(5);
		return 1;
	}
	
	if (m_error_code == "NO_DEVICE") {
		if (!m_netifd.protoSetAvail(m_iface, false)) {
			LOGE("Can't send available=false to netifd...\n");
			sleep(5);
		}
	} else {
		if (m_error_fatal) {
			if (!m_netifd.protoBlockRestart(m_iface)) {
				LOGE("Can't send restart blocking '%s' to netifd...\n", m_error_code.c_str());
				sleep(5);
			}
			
			if (!m_netifd.protoError(m_iface, m_error_code))
				LOGE("Can't send error '%s' to netifd...\n", m_error_code.c_str());
		} else {
			sleep(5);
			
			if (!m_netifd.protoError(m_iface, m_error_code))
				LOGE("Can't send error '%s' to netifd...\n", m_error_code.c_str());
		}
	}
	
	return 1;
}

void ModemService::intiUbusApi() {
	UbusLoop::setTimeout([this]() {
		m_api->setUbus(&m_ubus);
		m_api->setModem(m_modem);
		m_api->setSmsDb(&m_sms);
		
		if (!m_api->start())
			LOGE("Can't start API server, but continuing running...\n");
	}, 0);
}

int ModemService::run() {
	UbusLoop::instance()->init();
	Loop::instance()->init();
	
	if (init() && runModem()) {
		intiUbusApi();
		
		std::thread modem_thread([this]() {
			Loop::instance()->run();
		});
		
		UbusLoop::instance()->run();
		modem_thread.join();
		
		finishModem();
	}
	
	int diff = getCurrentTimestamp() - m_start_time;
	LOGD("Done, total uptime: %d ms\n", diff);
	
	return checkError();
}
