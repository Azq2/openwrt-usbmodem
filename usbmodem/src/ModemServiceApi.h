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
#include "ModemService.h"

class ModemService;

class ModemServiceApi {
	protected:
		Ubus *m_ubus = nullptr;
		Modem *m_modem = nullptr;
		ModemService *m_service = nullptr;
		
		struct DeferApiResult {
			int64_t time;
			json result;
			int status;
		};
		
		std::map<std::string, DeferApiResult> m_deferred_results;
		
		void reply(std::shared_ptr<UbusRequest> req, json result, int status = 0);
		void initApiRequest(std::shared_ptr<UbusRequest> req);
		
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
		
		// Modem API
		void apiGetModemInfo(std::shared_ptr<UbusRequest> req);
		void apiGetSimInfo(std::shared_ptr<UbusRequest> req);
		void apiGetNetworkInfo(std::shared_ptr<UbusRequest> req);
		void apiSendCommand(std::shared_ptr<UbusRequest> req);
		void apiSendUssd(std::shared_ptr<UbusRequest> req);
		void apiCancelUssd(std::shared_ptr<UbusRequest> req);
		void apiReadSms(std::shared_ptr<UbusRequest> req);
		void apiDeleteSms(std::shared_ptr<UbusRequest> req);
		void apiSearchOperators(std::shared_ptr<UbusRequest> req);
		void apiSetOperator(std::shared_ptr<UbusRequest> req);
		void apiGetSettings(std::shared_ptr<UbusRequest> req);
		void apiSetNetworkMode(std::shared_ptr<UbusRequest> req);
		
		// Internal API
		int apiGetDeferredResult(std::shared_ptr<UbusRequest> req);
	public:
		explicit ModemServiceApi(ModemService *service) : m_service(service) { }
		
		inline void setModem(Modem *modem) {
			m_modem = modem;
		}
		
		inline void setUbus(Ubus *ubus) {
			m_ubus = ubus;
		}
		
		bool start();
};
