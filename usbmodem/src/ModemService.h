#pragma once

#include <signal.h>
#include <pthread.h>
#include <map>
#include <string>

#include <Core/Log.h>
#include <Core/Loop.h>
#include <Core/Ubus.h>
#include <Core/Netifd.h>
#include <Core/SmsDb.h>

#include "Modem.h"
#include "ModemServiceApi.h"

class ModemServiceApi;

class ModemService {
	protected:
		enum SmsMode {
			SMS_MODE_MIRROR,
			SMS_MODE_DB
		};
		
	protected:
		Ubus m_ubus;
		Netifd m_netifd;
		Modem *m_modem = nullptr;
		ModemServiceApi *m_api = nullptr;
		SmsDb m_sms;
		
		std::map<std::string, std::string> m_uci_options;
		bool m_dhcp_inited = false;
		std::string m_iface;
		std::string m_firewall_zone;
		std::string m_error_code;
		bool m_error_fatal = false;
		bool m_manual_shutdown = false;
		struct sigaction m_sigaction = {};
		
		int64_t m_start_time = 0;
		int64_t m_last_connected = 0;
		int64_t m_last_disconnected = 0;
		
		int m_tty_speed = 0;
		std::string m_tty_path;
		std::string m_net_iface;
		std::string m_ppp_iface;
		
		SmsMode m_sms_mode = SMS_MODE_DB;
		
		bool validateOptions();
		bool startDhcp();
		bool stopDhcp();
		inline bool hasNetDev() {
			return (m_uci_options["modem_type"] == "asr1802" || m_uci_options["modem_type"] == "ncm");
		}
		
		bool setError(const std::string &code, bool fatal = false);
		
		int checkError();
		void intiUbusApi();
		void loadSmsFromModem();
	public:
		explicit ModemService(const std::string &iface);
		~ModemService();
		
		inline int64_t uptime() const {
			return getCurrentTimestamp() - m_start_time;
		}
		
		inline std::string iface() const {
			return m_iface;
		}
		
		static int run(const std::string &type, int argc, char *argv[]);
		
		bool init();
		bool runModem();
		void finishModem();
		int start();
};
