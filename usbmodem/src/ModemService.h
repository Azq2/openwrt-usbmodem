#pragma once

#include "Log.h"
#include "Modem.h"
#include "Loop.h"
#include "Ubus.h"
#include "Netifd.h"

#include <json-c/json.h>

#include <map>
#include <string>

class ModemService {
	protected:
		Ubus m_ubus;
		Netifd m_netifd;
		Modem *m_modem = nullptr;
		std::map<std::string, std::string> m_uci_options;
		bool m_dhcp_inited = false;
		std::string m_firewall_zone = "";
		
		int64_t m_start_time = 0;
		int64_t m_last_connected = 0;
		int64_t m_last_disconnected = 0;
		
		bool validateOptions();
		bool startDhcp(const std::string &iface);
		bool stopDhcp(const std::string &iface);
	public:
		ModemService();
		int run(const std::string &iface);
};
