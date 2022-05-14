#pragma once

#include <signal.h>
#include <pthread.h>
#include <map>
#include <string>

#include <Core/Log.h>
#include <Core/Loop.h>
#include <Core/Ubus.h>
#include <Core/Netifd.h>

#include "Modem.h"

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
		
		struct DeferApiResult {
			int64_t time;
			json result;
		};
		
		std::map<std::string, DeferApiResult> m_deferred_results;
		
		int64_t m_start_time = 0;
		int64_t m_last_connected = 0;
		int64_t m_last_disconnected = 0;
		
		int m_tty_speed = 0;
		std::string m_tty_path;
		std::string m_net_iface;
		
		bool validateOptions();
		bool startDhcp();
		bool stopDhcp();
		inline bool hasNetDev() {
			return (
				m_uci_options["modem_type"] == "asr1802" ||
				m_uci_options["modem_type"] == "ncm"
			);
		}
		
		inline void setDeferredResult(std::string deferred_id, json result) {
			m_deferred_results[deferred_id].time = getCurrentTimestamp();
			m_deferred_results[deferred_id].result = result;
		}
		
		inline std::string enableDefferedResult(std::shared_ptr<UbusRequest> req, bool enable) {
			if (enable) {
				std::string id = strprintf("%llu", getCurrentTimestamp());
				req->reply({
					{"deferred", id}
				});
				m_deferred_results[id] = {.time = 0, .result = {}};
				return id;
			} else {
				req->defer();
				return "";
			}
		}
		
		inline std::string getStrArg(const json &params, std::string key, std::string default_value = "") {
			return params[key].is_string() ? params[key].get<std::string>() : default_value;
		}
		
		inline int getIntArg(const json &params, std::string key, int default_value = 0) {
			if (params[key].is_string())
				return std::stoi(params[key].get<std::string>());
			return params[key].is_number() ? params[key].get<int>() : default_value;
		}
		
		inline bool getBoolArg(const json &params, std::string key, bool default_value = false) {
			return params[key].is_boolean() ? params[key].get<bool>() : default_value;
		}
		
		bool setError(const std::string &code, bool fatal = false);
		
		int checkError();
		
		// API
		int apiGetInfo(std::shared_ptr<UbusRequest> req);
		int apiSendCommand(std::shared_ptr<UbusRequest> req);
		int apiSendUssd(std::shared_ptr<UbusRequest> req);
		int apiCancelUssd(std::shared_ptr<UbusRequest> req);
		int apiReadSms(std::shared_ptr<UbusRequest> req);
		int apiDeleteSms(std::shared_ptr<UbusRequest> req);
		int apiSearchOperators(std::shared_ptr<UbusRequest> req);
		int apiSetOperator(std::shared_ptr<UbusRequest> req);
		int apiGetDeferredResult(std::shared_ptr<UbusRequest> req);
		int apiGetSettings(std::shared_ptr<UbusRequest> req);
		int apiSetNetworkMode(std::shared_ptr<UbusRequest> req);
	public:
		explicit ModemService(const std::string &iface);
		
		bool init();
		bool runModem();
		bool runApi();
		void finishModem();
		int run();
};
