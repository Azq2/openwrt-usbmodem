#include "ModemService.h"

#include <vector>
#include <csignal>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

#include "Modem/Asr1802.h"
#include "Modem/GenericPpp.h"
#include "Modem/HuaweiNcm.h"

bool ModemService::runModem() {
	switch (m_type) {
		case UsbDiscover::TYPE_NCM:
			m_modem = new HuaweiNcmModem();
		break;
		
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
	m_modem->setOption<int>("tty_baudrate", m_control_tty_baudrate);
	
	// PDP config
	m_modem->setOption<std::string>("pdp_type", m_options["pdp_type"]);
	m_modem->setOption<std::string>("pdp_apn", m_options["apn"]);
	m_modem->setOption<std::string>("pdp_auth_mode", m_options["auth_type"]);
	m_modem->setOption<std::string>("pdp_user", m_options["username"]);
	m_modem->setOption<std::string>("pdp_password", m_options["password"]);
	
	// Security codes
	m_modem->setOption<std::string>("pin_code", m_options["pin_code"]);
	m_modem->setOption<std::string>("mep_code", m_options["mep_code"]);
	
	// Other settings
	m_modem->setOption<bool>("allow_roaming", getBoolOption(m_options["allow_roaming"]));
	m_modem->setOption<bool>("prefer_dhcp", getBoolOption(m_options["prefer_dhcp"]));
	
	if (m_options["sms_storage"] == "sim") {
		m_modem->setOption<Modem::SmsPreferredStorage>("sms_storage", Modem::SMS_PREFER_SIM);
	} else if (m_options["sms_storage"] == "modem") {
		m_modem->setOption<Modem::SmsPreferredStorage>("sms_storage", Modem::SMS_PREFER_MODEM);
	} else {
		m_modem->setOption<Modem::SmsPreferredStorage>("sms_storage", Modem::SMS_PREFER_EXTERNAL);
	}
	
	m_modem->setOption<std::string>("modem_init", m_options["modem_init"]);
	
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
	
	m_modem->on<Modem::EvSimStateChanged>([this](const auto &event) {
		LOGD("[sim] %s\n", Modem::getEnumName(event.state, true));
	});
	
	m_modem->on<Modem::EvIoBroken>([this](const auto &event) {
		LOGE("TTY device is lost...\n");
		setError("USBMODEM_INTERNAL_ERROR");
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
