#pragma once

#include "Log.h"
#include "Modem.h"
#include "Loop.h"
#include "Ubus.h"
#include "Netifd.h"

#include <signal.h>
#include <json-c/json.h>
#include <pthread.h>

#include <map>
#include <string>

class ModemService {
	protected:
		Ubus m_ubus;
		Netifd m_netifd;
		Modem *m_modem = nullptr;
		std::map<std::string, std::string> m_uci_options;
		bool m_dhcp_inited = false;
		std::string m_iface;
		std::string m_firewall_zone;
		std::string m_error_code;
		bool m_error_fatal = false;
		struct sigaction m_sigaction = {};
		
		int64_t m_start_time = 0;
		int64_t m_last_connected = 0;
		int64_t m_last_disconnected = 0;
		
		int m_tty_speed = 0;
		std::string m_tty_path;
		std::string m_net_iface;
		
		bool validateOptions();
		bool startDhcp();
		bool stopDhcp();
		
		bool setError(const std::string &code, bool fatal = false);
		
		int checkError();
	public:
		explicit ModemService(const std::string &iface);
		
		bool init();
		bool runModem();
		void finishModem();
		int run();
};
