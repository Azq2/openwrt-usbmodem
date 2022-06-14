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
#include "UsbDiscover.h"

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
		
		UsbDiscover::ModemType m_type = UsbDiscover::TYPE_UNKNOWN;
		
		std::map<std::string, std::string> m_options;
		
		std::string m_iface;
		std::string m_fw_zone;
		std::string m_control_tty, m_ppp_tty, m_net_dev;
		int m_control_tty_speed = 0, m_ppp_tty_speed = 0;
		
		bool m_dhcp_inited = false;
		std::string m_error_code;
		bool m_error_fatal = false;
		bool m_manual_shutdown = false;
		struct sigaction m_sigaction = {};
		
		int64_t m_start_time = 0;
		int64_t m_last_connected = 0;
		int64_t m_last_disconnected = 0;
		
		SmsMode m_sms_mode = SMS_MODE_DB;
		
		bool loadOptions();
		bool resolveDevices(bool lock);
		
		bool startDhcp();
		bool stopDhcp();
		
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
		bool check();
		bool runModem();
		void finishModem();
		int start();
};
