#include "ModemService.h"

#include <vector>
#include <csignal>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

ModemService::ModemService(const std::string &iface): m_iface(iface) {
	m_start_time = getCurrentTimestamp();
	m_api = new ModemServiceApi(this);
}

bool ModemService::loadOptions() {
	m_options = {
		{"proto", ""},
		
		{"modem_type", ""},
		
		{"control_device", ""},
		{"control_device_baudrate", "115200"},
		
		{"ppp_device", ""},
		{"ppp_device_baudrate", "115200"},
		
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
	m_control_tty_baudrate = strToInt(m_options["control_device_baudrate"], 10, 0);
	m_ppp_tty_baudrate = strToInt(m_options["ppp_device_baudrate"], 10, 0);
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

ModemService::~ModemService() {
	if (m_api)
		delete m_api;
	
	if (m_modem)
		delete m_modem;
}
