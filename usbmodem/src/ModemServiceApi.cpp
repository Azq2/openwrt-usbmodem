#include "ModemServiceApi.h"

#include <Core/AtChannel.h>
#include <Core/GsmUtils.h>
#include <Core/UbusLoop.h>

static std::vector<Modem::NetworkTech> ALL_NETWORK_TECH_LIST = {
	Modem::TECH_UNKNOWN,
	Modem::TECH_NO_SERVICE,
	Modem::TECH_GSM,
	Modem::TECH_GSM_COMPACT,
	Modem::TECH_GPRS,
	Modem::TECH_EDGE,
	Modem::TECH_UMTS,
	Modem::TECH_HSDPA,
	Modem::TECH_HSUPA,
	Modem::TECH_HSPA,
	Modem::TECH_HSPAP,
	Modem::TECH_LTE
};

void ModemServiceApi::apiGetModemInfo(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=, this]() {
		auto [success, modem_info] = m_modem->getModemInfo();
		if (!success) {
			reply(req, {
				{"error", "Can't get modem info"}
			});
			return;
		}
		
		reply(req, {
			{"uptime", m_service->uptime()},
			{"vendor", modem_info.vendor},
			{"model", modem_info.model},
			{"version", modem_info.version},
			{"imei", modem_info.imei}
		});
	}, 0);
}

void ModemServiceApi::apiGetSimInfo(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=, this]() {
		auto [success, sim_info] = m_modem->getSimInfo();
		if (!success) {
			reply(req, {
				{"error", "Can't get modem info"}
			});
			return;
		}
		
		reply(req, {
			{"imsi", sim_info.imsi},
			{"number", sim_info.number},
			{"state", Modem::getEnumName(sim_info.state)}
		});
	}, 0);
}

void ModemServiceApi::apiGetNetworkInfo(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=, this]() {
		auto [success, net_info] = m_modem->getNetworkInfo();
		if (!success) {
			reply(req, {
				{"error", "Can't get modem info"}
			});
			return;
		}
		
		double quality = 0;
		if (!std::isnan(net_info.signal.rssi_dbm))
			quality = rssiToPercent(net_info.signal.rssi_dbm, -100, -50);
		
		reply(req, {
			{"ipv4", {
				{"ip", net_info.ipv4.ip},
				{"mask", net_info.ipv4.mask},
				{"gw", net_info.ipv4.gw},
				{"dns1", net_info.ipv4.dns1},
				{"dns2", net_info.ipv4.dns2},
			}},
			{"ipv6", {
				{"ip", net_info.ipv6.ip},
				{"mask", net_info.ipv6.mask},
				{"gw", net_info.ipv6.gw},
				{"dns1", net_info.ipv6.dns1},
				{"dns2", net_info.ipv6.dns2},
			}},
			{"signal", {
				{"rssi_dbm", net_info.signal.rssi_dbm},
				{"bit_err_pct", net_info.signal.bit_err_pct},
				{"rscp_dbm", net_info.signal.rscp_dbm},
				{"ecio_db", net_info.signal.ecio_db},
				{"rsrq_db", net_info.signal.rsrq_db},
				{"rsrp_dbm", net_info.signal.rsrp_dbm},
				{"quality", quality}
			}},
			{"operator", {
				{"registration", Modem::getEnumName(net_info.oper.reg)},
				{"mcc", net_info.oper.mcc},
				{"mnc", net_info.oper.mnc},
				{"name", net_info.oper.name},
				{"tech", Modem::getEnumName(net_info.oper.tech)},
				{"status", Modem::getEnumName(net_info.oper.status)},
			}},
			{"tech", Modem::getEnumName(net_info.tech)},
			{"registration", Modem::getEnumName(net_info.reg)},
		});
	}, 0);
}

void ModemServiceApi::apiSendUssd(std::shared_ptr<UbusRequest> req) {
	const auto &params = req->data();
	
	bool is_answer = true;
	int timeout = getIntArg(params, "timeout", 0);
	std::string query = getStrArg(params, "answer", "");
	
	if (!query.size()) {
		is_answer = false;
		query = getStrArg(params, "query", "");
	}
	
	Loop::setTimeout([=, this]() {
		if (!is_answer && m_modem->isUssdWaitReply())
			m_modem->cancelUssd();
		
		m_modem->sendUssd(query, [=, this](Modem::UssdCode code, const std::string &response) {
			if (code == Modem::USSD_ERROR) {
				reply(req, {{"error", response}});
			} else {
				reply(req, {
					{"code", code},
					{"response", response}
				});
			}
		}, timeout);
	}, 0);
}

void ModemServiceApi::apiCancelUssd(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=, this]() {
		if (!m_modem->cancelUssd()) {
			reply(req, {{"error", "Can't cancel USSD."}});
		} else {
			reply(req, {});
		}
	}, 0);
}

void ModemServiceApi::apiSendCommand(std::shared_ptr<UbusRequest> req) {
	const auto &params = req->data();
	
	int timeout = getIntArg(params, "timeout", 0);
	std::string cmd = getStrArg(params, "command", "");
	
	if (!cmd.size() > 0) {
		reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
		return;
	}
	
	Loop::setTimeout([=, this]() {
		auto [success, response] = m_modem->sendAtCommand(cmd, timeout);
		reply(req, {
			{"success", success},
			{"response", response},
		});
	}, 0);
}

void ModemServiceApi::apiReadSms(std::shared_ptr<UbusRequest> req) {
	static std::map<Modem::SmsStorage, std::string> storage_names = {
		{Modem::SMS_STORAGE_MT, "MT"},
		{Modem::SMS_STORAGE_ME, "ME"},
		{Modem::SMS_STORAGE_SM, "SM"},
	};
	
	const auto &params = req->data();
	auto dir = static_cast<Modem::SmsDir>(getIntArg(params, "dir", Modem::SMS_DIR_ALL));
	
	Loop::setTimeout([=, this]() {
		auto [status, list] = m_modem->getSmsList(dir);
		if (!status) {
			reply(req, {{"error", "Can't get SMS from modem."}});
			return;
		}
		
		Modem::SmsStorageCapacity capacity = m_modem->getSmsCapacity();
		Modem::SmsStorage storage = m_modem->getSmsStorage();
		
		std::string storage_id = "UNKNOWN";
		if (storage_names.find(storage) != storage_names.cend())
			storage_id = storage_names[storage];
		
		json response = {
			{"capacity", {
				{"used", capacity.used},
				{"total", capacity.total}
			}},
			{"storage", storage_id},
			{"messages", json::array()}
		};
		
		for (auto &sms: list) {
			json message = {
				{"hash", sms.hash},
				{"addr", sms.addr},
				{"time", sms.time},
				{"type", sms.type},
				{"unread", sms.unread},
				{"invalid", sms.invalid},
				{"dir", sms.dir},
				{"parts", json::array()}
			};
			
			for (auto &part: sms.parts) {
				message["parts"].push_back({
					{"id", part.id},
					{"text", part.text}
				});
			}
			
			response["messages"].push_back(message);
		}
		
		reply(req, response);
	}, 0);
}

void ModemServiceApi::apiDeleteSms(std::shared_ptr<UbusRequest> req) {
	const auto &params = req->data();
	
	if (!params["ids"].is_array() || !params["ids"].size()) {
		reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
		return;
	}
	
	std::vector<int> message_ids;
	
	for (auto &item: params["ids"]) {
		if (!item.is_number()) {
			reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
			return;
		}
		message_ids.push_back(item.get<int>());
	}
	
	Loop::setTimeout([=, this]() {
		json response = {
			{"result", json::object()},
			{"errors", json::object()},
		};
		
		for (auto id: message_ids) {
			if (!m_modem->deleteSms(id)) {
				response["result"][std::to_string(id)] = false;
				response["errors"][std::to_string(id)] = strprintf("Message #%d failed to delete.", id);
			} else {
				response["result"][std::to_string(id)] = true;
			}
		}
		
		reply(req, response);
	}, 0);
}

void ModemServiceApi::apiSearchOperators(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=, this]() {
		auto [success, list] = m_modem->searchOperators();
		if (!success) {
			reply(req, {
				{"error", "Can't get modem info"}
			});
			return;
		}
		
		json response = {
			{"list", json::array()}
		};
		for (auto &op: list) {
			response["list"].push_back({
				{"mcc", op.mcc},
				{"mnc", op.mnc},
				{"name", op.name},
				{"status", Modem::getEnumName(op.status)},
				{"tech", Modem::getEnumName(op.tech)},
			});
		}
		reply(req, response);
	}, 0);
}

void ModemServiceApi::apiSetOperator(std::shared_ptr<UbusRequest> req) {
	const auto &params = req->data();
	std::string mode = getStrArg(params, "mode", "auto");
	std::string tech_name = getStrArg(params, "tech", "");
	int mcc = getIntArg(params, "mcc", 0);
	int mnc = getIntArg(params, "mnc", 0);
	
	Modem::NetworkTech tech = Modem::TECH_UNKNOWN;
	for (auto v: ALL_NETWORK_TECH_LIST) {
		if (strcasecmp(Modem::getEnumName(v), tech_name.c_str()) == 0) {
			tech = v;
			break;
		}
	}
	
	LOGD("apiSetOperator!!!!!!!!!!\n");
	
	Loop::setTimeout([=, this]() {
		json response = {{"success", false}};
		if (mode == "manual") {
			response["success"] = m_modem->setOperator(Modem::OPERATOR_REG_MANUAL, mcc, mnc, tech);
		} else if (mode == "auto") {
			response["success"] = m_modem->setOperator(Modem::OPERATOR_REG_AUTO);
		} else if (mode == "none") {
			response["success"] = m_modem->setOperator(Modem::OPERATOR_REG_NONE);
		} else {
			response["error"] = "Invalid mode.";
		}
		reply(req, response);
	}, 0);
}

int ModemServiceApi::apiGetDeferredResult(std::shared_ptr<UbusRequest> req) {
	json response = {};
	const auto &params = req->data();
	std::string id = getStrArg(params, "id", "");
	
	auto it = m_deferred_results.find(id);
	if (it != m_deferred_results.end()) {
		response["result"] = it->second.result;
		response["ready"] = it->second.time > 0;
		response["exists"] = true;
		
		if (it->second.time > 0)
			m_deferred_results.erase(id);
	} else {
		response["exists"] = false;
	}
	
	req->reply(response);
	
	return 0;
}

void ModemServiceApi::apiGetSettings(std::shared_ptr<UbusRequest> req) {
	/*
	const auto &params = req->data();
	bool async = getBoolArg(params, "async", false);
	
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=, this]() {
		json response = {};
		response["network_modes"] = json::array();
		
		int mode_id = m_modem->getCurrentModeId();
		for (auto &mode: m_modem->getAvailableNetworkModes()) {
			response["network_modes"].push_back({
				{"id", mode.id},
				{"name", mode.name}
			});
		}
		
		response["network_mode"] = mode_id;
		response["roaming_enabled"] = m_modem->isRoamingEnabled();
		
		if (async) {
			setDeferredResult(deferred_id, response);
		} else {
			req->reply(response);
		}
	}, 0);
	*/
	reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
}

void ModemServiceApi::apiSetNetworkMode(std::shared_ptr<UbusRequest> req) {
	/*
	const auto &params = req->data();
	bool async = getBoolArg(params, "async", false);
	bool roaming = getBoolArg(params, "roaming", false);
	int mode_id = getIntArg(params, "mode", -1);
	
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=, this]() {
		json response = {{"success", true}};
		
		if (!m_modem->setNetworkMode(mode_id)) {
			response["success"] = false;
			response["error"] = "Can't set network mode";
		} else if (!m_modem->setDataRoaming(roaming)) {
			response["success"] = false;
			response["error"] = "Can't set data roaming";
		}
		
		if (async) {
			setDeferredResult(deferred_id, response);
		} else {
			req->reply(response);
		}
	}, 0);
	*/
	reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
}

void ModemServiceApi::reply(std::shared_ptr<UbusRequest> req, json result, int status) {
	if (m_deferred_results.find(req->uniqKey()) != m_deferred_results.end()) {
		m_deferred_results[req->uniqKey()].time = getCurrentTimestamp();
		m_deferred_results[req->uniqKey()].result = result;
		m_deferred_results[req->uniqKey()].status = status;
	} else {
		UbusLoop::setTimeout([=, this]() {
			req->reply(result, status);
		}, 0);
	}
}

void ModemServiceApi::initApiRequest(std::shared_ptr<UbusRequest> req) {
	req->defer();
	if (getBoolArg(req->data(), "async", false)) {
		UbusLoop::setTimeout([this, req]() {
			if (!req->done()) {
				req->reply({
					{"deferred", req->uniqKey()}
				});
				m_deferred_results[req->uniqKey()] = {.time = 0, .result = {}};
			}
		}, 5000);
	}
}

bool ModemServiceApi::start() {
	return m_ubus->object("usbmodem." + m_service->iface())
		.method("getDeferredResult", [this](auto req) {
			return apiGetDeferredResult(req);
		})
		
		.method("getModemInfo", [this](auto req) {
			initApiRequest(req);
			apiGetModemInfo(req);
			return 0;
		})
		
		.method("getNetworkInfo", [this](auto req) {
			initApiRequest(req);
			apiGetNetworkInfo(req);
			return 0;
		})
		
		.method("getSimInfo", [this](auto req) {
			initApiRequest(req);
			apiGetSimInfo(req);
			return 0;
		})
		
		.method("searchOperators", [this](auto req) {
			initApiRequest(req);
			apiSearchOperators(req);
			return 0;
		})
		
		.method("setOperator", [this](auto req) {
			initApiRequest(req);
			apiSetOperator(req);
			return 0;
		}, {
			{"id", UbusObject::STRING},
			{"tech", UbusObject::INT32}
		})
		
		.method("get_settings", [this](auto req) {
			initApiRequest(req);
			apiGetSettings(req);
			return 0;
		})
		.method("send_command", [this](auto req) {
			initApiRequest(req);
			apiSendCommand(req);
			return 0;
		}, {
			{"command", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.method("send_ussd", [this](auto req) {
			initApiRequest(req);
			apiSendUssd(req);
			return 0;
		}, {
			{"query", UbusObject::STRING},
			{"answer", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.method("cancel_ussd", [this](auto req) {
			initApiRequest(req);
			apiCancelUssd(req);
			return 0;
		})
		.method("read_sms", [this](auto req) {
			initApiRequest(req);
			apiReadSms(req);
			return 0;
		}, {
			{"dir", UbusObject::STRING}
		})
		.method("delete_sms", [this](auto req) {
			initApiRequest(req);
			apiDeleteSms(req);
			return 0;
		}, {
			{"ids", UbusObject::ARRAY}
		})
		.method("set_network_mode", [this](auto req) {
			initApiRequest(req);
			apiSetNetworkMode(req);
			return 0;
		})
		.attach();
}
