#include "ModemService.h"

#include <vector>
#include <csignal>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

#include "Modem/Asr1802.h"
#include "Modem/GenericPpp.h"

ModemService::ModemService(const std::string &iface): m_iface(iface) {
	m_start_time = getCurrentTimestamp();
	m_api = new ModemServiceApi(this);
}

bool ModemService::loadOptions() {
	m_options = {
		{"proto", ""},
		
		{"modem_type", ""},
		
		{"control_device", ""},
		{"control_device_speed", "115200"},
		
		{"ppp_device", ""},
		{"ppp_device_speed", "115200"},
		
		{"pdp_type", "IP"},
		{"apn", "internet"},
		{"dialnumber", ""},
		
		{"auth_type", ""},
		{"username", ""},
		{"password", ""},
		
		{"pin_code", ""},
		{"mep_code", ""},
	};
	
	auto [section_found, section] = Uci::loadSectionByName("network", "interface", m_iface);
	if (!section_found) {
		LOGE("Can't found config for interface: %s\n", m_iface.c_str());
		return false;
	}
	
	for (auto &it: section.options)
		m_options[it.first] = it.second;
	
	if (m_options["proto"] != "usbmodem") {
		LOGE("Uunsupported protocol (%s) for interface %s.\n", m_options["proto"].c_str(), m_iface.c_str());
		return false;
	}
	
	if (m_options["pdp_type"] != "IP" && m_options["pdp_type"] != "IPV6" && m_options["pdp_type"] != "IPV4V6") {
		LOGE("Invalid PDP type (%s) for interface %s.\n", m_options["pdp_type"].c_str(), m_iface.c_str());
		return false;
	}
	
	if (m_options["auth_type"] != "" && m_options["auth_type"] != "pap" && m_options["auth_type"] != "chap") {
		LOGE("Invalid auth type (%s) for interface %s.\n", m_options["auth_type"].c_str(), m_iface.c_str());
		return false;
	}
	
	m_type = UsbDiscover::getModemTypeFromString(m_options["modem_type"]);
	if (m_type == UsbDiscover::TYPE_UNKNOWN) {
		LOGD("Unknown modem type (%s) for interface %s.\n ", m_options["modem_type"].c_str(), m_iface.c_str());
		return false;
	}
	
	if (UsbDiscover::hasControlDev(m_type) && !m_options["control_device"].size()) {
		LOGD("Please, specify 'control_device' for interface %s.\n", m_iface.c_str());
		return false;
	}
	
	if (UsbDiscover::hasPppDev(m_type) && !m_options["ppp_device"].size()) {
		LOGD("Please, specify 'ppp_device' for interface %s.\n", m_iface.c_str());
		return false;
	}
	
	if (UsbDiscover::hasNetDev(m_type) && !m_options["net_device"].size()) {
		LOGD("Please, specify 'net_device' for interface %s.\n", m_iface.c_str());
		return false;
	}
	
	return true;
}

bool ModemService::resolveDevices(bool lock) {
	m_control_tty_speed = strToInt(m_options["control_device_speed"], 10, 0);
	m_ppp_tty_speed = strToInt(m_options["ppp_device_speed"], 10, 0);
	m_fw_zone = Uci::getFirewallZone(m_iface);
	
	if (m_options["device"] != "" && m_options["device"] != "tty" && m_options["device"] != "custom") {
		auto [found, dev] = UsbDiscover::findDevice(m_options["device"], m_iface);
		if (!found) {
			LOGE("Device not found: %s\n", m_options["device"].c_str());
			return false;
		}
		
		m_control_tty = UsbDiscover::getFromDevice(dev, m_options["control_device"]);
		m_ppp_tty = UsbDiscover::getFromDevice(dev, m_options["ppp_device"]);
		m_net_dev = UsbDiscover::getFromDevice(dev, m_options["net_device"]);
	} else {
		m_control_tty = m_options["control_device"];
		m_ppp_tty = m_options["ppp_device"];
		m_net_dev = m_options["net_device"];
	}
	
	if (UsbDiscover::hasControlDev(m_type)) {
		if (!m_control_tty.size() || !isFileExists(m_control_tty)) {
			LOGE("Control device not found: %s\n",  m_options["control_device"].c_str());
			return false;
		}
		
		if (lock && !UsbDiscover::tryLockDevice(m_control_tty, m_iface)) {
			LOGE("Control device already in use: %s\n", m_control_tty.c_str());
			return false;
		}
	}
	
	if (UsbDiscover::hasPppDev(m_type)) {
		if (!m_ppp_tty.size() || !isFileExists(m_ppp_tty)) {
			LOGE("PPP device not found: %s\n",  m_options["ppp_device"].c_str());
			return false;
		}
		
		if (lock && !UsbDiscover::tryLockDevice(m_ppp_tty, m_iface)) {
			LOGE("PPP device already in use: %s\n", m_ppp_tty.c_str());
			return false;
		}
	}
	
	if (UsbDiscover::hasNetDev(m_type)) {
		if (!m_net_dev.size() || !isFileExists("/sys/class/net/" + m_net_dev)) {
			LOGE("NET device not found: %s\n",  m_options["net_device"].c_str());
			return false;
		}
		
		if (lock && !UsbDiscover::tryLockDevice(m_net_dev, m_iface)) {
			LOGE("NET device already in use: %s\n", m_net_dev.c_str());
			return false;
		}
	}
	
	return true;
}

bool ModemService::check() {
	if (!m_ubus.open()) {
		LOGE("Can't init ubus...\n");
		return false;
	}
	
	m_netifd.setUbus(&m_ubus);
	
	if (!loadOptions()) 
		return false;
	
	if (!resolveDevices(false))
		return false;
	
	m_netifd.protoSetAvail(m_iface, true);
	return true;
}

bool ModemService::init() {
	if (!m_ubus.open()) {
		LOGE("Can't init ubus...\n");
		return setError("USBMODEM_INTERNAL_ERROR", true);
	}
	
	m_netifd.setUbus(&m_ubus);
	
	if (!loadOptions())
		return setError("USBMODEM_INVALID_CONFIG", true);
	
	if (!resolveDevices(true))
		return setError("NO_DEVICE");
	
	if (UsbDiscover::hasNetDev(m_type)) {
		// Link modem interface to main interface
		if (!m_netifd.updateIface(m_iface, m_net_dev, nullptr, nullptr)) {
			LOGE("Can't init iface...\n");
			return setError("USBMODEM_INTERNAL_ERROR");
		}
	}
	
	return true;
}

void ModemService::loadSmsFromModem() {
	switch (m_sms_mode) {
		case SMS_MODE_MIRROR:
		{
			if (!m_sms.ready()) {
				if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_MT) {
					m_sms.setStorageType(SmsDb::STORAGE_SIM_AND_MODEM);
				} else if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_ME) {
					m_sms.setStorageType(SmsDb::STORAGE_MODEM);
				} else if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_SM) {
					m_sms.setStorageType(SmsDb::STORAGE_SIM);
				} else {
					LOGE("Unknown storage type!\n");
					return;
				}
				
				auto capacity = m_modem->getSmsCapacity();
				m_sms.setMaxCapacity(capacity.total);
				
				m_sms.init();
				
				// Loading all messages to DB
				auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_ALL);
				if (!success || !m_sms.add(messages))
					LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			} else {
				// Loading unread messages to DB
				auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_UNREAD);
				if (!success || !m_sms.add(messages))
					LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			}
		}
		break;
		
		case SMS_MODE_DB:
		{
			if (!m_sms.ready()) {
				m_sms.setStorageType(SmsDb::STORAGE_FILESYSTEM);
				m_sms.init();
				
				if (!m_sms.load()) {
					LOGE("[sms] Failed to load sms database.\n");
				}
			}
			
			// Loading all messages to DB
			auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_ALL);
			if (success && m_sms.add(messages)) {
				if (m_sms.save()) {
					// And now deleting all read SMS, because we need enough storage for new messages
					if (!m_modem->deleteReadedSms())
						LOGE("[sms] Failed to delete already readed SMS.\n");
				} else {
					LOGE("[sms] Failed to save sms database.\n");
				}
			} else {
				LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			}
		}
		break;
	}
}

bool ModemService::runModem() {
	switch (m_type) {
		case UsbDiscover::TYPE_ASR1802:
			m_modem = new Asr1802Modem();
		break;
		
		case UsbDiscover::TYPE_PPP:
			m_modem = new GenericPppModem();
		break;
		
		default:
			LOGE("Unsupported modem type: %s\n", m_options["modem_type"].c_str());
			return setError("USBMODEM_INVALID_CONFIG", true);
		break;
	}
	
	// Device config
	m_modem->setOption<std::string>("tty_device", m_control_tty);
	m_modem->setOption<int>("tty_speed", m_control_tty_speed);
	
	// PDP config
	m_modem->setOption<std::string>("pdp_type", m_options["pdp_type"]);
	m_modem->setOption<std::string>("pdp_apn", m_options["apn"]);
	m_modem->setOption<std::string>("pdp_auth_mode", m_options["auth_type"]);
	m_modem->setOption<std::string>("pdp_user", m_options["username"]);
	m_modem->setOption<std::string>("pdp_password", m_options["password"]);
	
	// Security codes
	m_modem->setOption<std::string>("pincode", m_options["pincode"]);
	
	// Other settings
	m_modem->setOption<bool>("prefer_dhcp", m_options["prefer_dhcp"] == "1");
	m_modem->setOption<bool>("prefer_sms_to_sim", m_options["prefer_sms_to_sim"] == "1");
	m_modem->setOption<int>("connect_timeout", strToInt(m_options["connect_timeout"]) * 1000);
	
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
			if (!m_netifd.updateIface(m_iface, m_net_dev, &event.ipv4, &event.ipv6)) {
				LOGE("Can't set IP to interface '%s'\n", m_iface.c_str());
				setError("USBMODEM_INTERNAL_ERROR");
			}
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			if (dhcp_delay > 0) {
				LOGD("Wait %d ms for DHCP recovery...\n", dhcp_delay);
				Loop::setTimeout([this]() {
					if (!startDhcp())
						setError("USBMODEM_INTERNAL_ERROR");
				}, dhcp_delay);
			} else {
				if (!startDhcp())
					setError("USBMODEM_INTERNAL_ERROR");
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
			if (!m_netifd.updateIface(m_iface, m_net_dev, nullptr, nullptr)) {
				LOGE("Can't set IP to interface '%s'\n", m_iface.c_str());
				setError("USBMODEM_INTERNAL_ERROR");
			}
		} else if (m_modem->getIfaceProto() == Modem::IFACE_DHCP) {
			if (!stopDhcp()) {
				setError("USBMODEM_INTERNAL_ERROR");
			}
		}
	});
	
	m_modem->on<Modem::EvSimStateChaned>([this](const auto &event) {
		LOGD("[sim] %s\n", Modem::getEnumName(event.state, true));
	});
	
	m_modem->on<Modem::EvIoBroken>([this](const auto &event) {
		LOGE("TTY device is lost...\n");
		setError("USBMODEM_INTERNAL_ERROR");
	});
	
	m_modem->on<Modem::EvDataConnectTimeout>([this](const auto &event) {
		LOGE("Internet connection timeout...\n");
	});
	
	m_modem->on<Modem::EvSmsReady>([this](const auto &event) {
		LOGD("[sms] SMS subsystem ready!\n");
		Loop::setTimeout([this]() {
			loadSmsFromModem();
		}, 0);
	});
	
	m_modem->on<Modem::EvNewDecodedSms>([this](const auto &event) {
		LOGD("[sms] received new sms! (decoded)\n");
		
	});
	
	m_modem->on<Modem::EvNewStoredSms>([this](const auto &event) {
		LOGD("[sms] received new sms! (stored)\n");
		Loop::setTimeout([this]() {
			loadSmsFromModem();
		}, 0);
	});
	
	if (!m_modem->open()) {
		LOGE("Can't initialize modem.\n");
		return setError("USBMODEM_INTERNAL_ERROR");
	}
	
	return true;
}

void ModemService::finishModem() {
	if (m_modem)
		m_modem->close();
}

bool ModemService::setError(const std::string &code, bool fatal) {
	m_error_code = code;
	m_error_fatal = fatal;
	
	Loop::instance()->stop();
	UbusLoop::instance()->stop();
	
	return false;
}

int ModemService::checkError() {
	if (!m_error_code.size() || m_manual_shutdown)
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

int ModemService::start() {
	UbusLoop::instance()->init();
	Loop::instance()->init();
	
	auto handler = [this](int sig) {
		LOGD("Received signal: %d\n", sig);
		m_manual_shutdown = true;
		Loop::instance()->stop();
		UbusLoop::instance()->stop();
	};
	setSignalHandler(SIGINT, handler);
	setSignalHandler(SIGTERM, handler);
	
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

int ModemService::run(const std::string &type, int argc, char *argv[]) {
	if (type == "daemon") {
		if (!argc) {
			LOGD("usage: usbmodem daemon <iface>\n");
			return 1;
		}
		
		ModemService s(argv[0]);
		return s.start();
	} else if (type == "check") {
		auto sections = Uci::loadSections("network", "interface");
		for (auto &section: sections) {
			if (getMapValue(section.options, "proto", "") == "usbmodem") {
				ModemService s(section.name);
				auto found = s.check();
				LOGD("-> %s: %s\n", section.name.c_str(), found ? "Available" : "Not available");
			}
		}
	}
	return 1;
}

bool ModemService::startDhcp() {
	if (m_dhcp_inited) {
		if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_4")) {
				LOGE("Can't send dhcp renew for '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_6")) {
				LOGE("Can't send dhcp renew for '%s_6'\n", m_iface.c_str());
				return false;
			}
		}
	} else {
		if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcp", m_iface + "_4", m_iface, m_fw_zone, m_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcpv6", m_iface + "_4", m_iface, m_fw_zone, m_options)) {
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
	
	if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_4")) {
			LOGE("Can't send dhcp release for '%s_4'\n", m_iface.c_str());
			return false;
		}
	}
	
	if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_6")) {
			LOGE("Can't send dhcp release for '%s_6'\n", m_iface.c_str());
			return false;
		}
	}
	
	return true;
}

ModemService::~ModemService() {
	if (m_api)
		delete m_api;
	
	if (m_modem)
		delete m_modem;
}
