#include "ModemService.h"
#include "AtChannel.h"
#include "GsmUtils.h"

static std::map<Modem::OperatorRegStatus, std::string> REG_STATUS_NAMES = {
	{Modem::OPERATOR_REG_NONE, "none"},
	{Modem::OPERATOR_REG_AUTO, "auto"},
	{Modem::OPERATOR_REG_MANUAL, "manual"},
};

static std::map<Modem::OperatorStatus, std::string> OPERATOR_STATUS_NAMES = {
	{Modem::OPERATOR_STATUS_UNKNOWN, "unknown"},
	{Modem::OPERATOR_STATUS_AVAILABLE, "available"},
	{Modem::OPERATOR_STATUS_REGISTERED, "registered"},
	{Modem::OPERATOR_STATUS_FORBIDDEN, "forbidden"},
};

int ModemService::apiSendUssd(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	bool is_answer = false;
	int timeout = 0;
	std::string query;
	
	if (params["query"].is_string()) {
		query = params["query"];
	} else if (params["answer"].is_string()) {
		is_answer = true;
		query = params["answer"];
	}
	
	if (params["timeout"].is_number())
		timeout = params["timeout"];
	
	if (!is_answer && m_modem->isUssdWaitReply())
		m_modem->cancelUssd();
	
	req->defer();
	
	m_modem->sendUssd(query, [=](Modem::UssdCode code, const std::string &response) {
		if (code == Modem::USSD_ERROR) {
			req->reply({{"error", response}});
		} else {
			req->reply({
				{"code", code},
				{"response", response}
			});
		}
	}, timeout);
	
	return 0;
}

int ModemService::apiCancelUssd(std::shared_ptr<UbusRequest> req) {
	if (!m_modem->cancelUssd())
		req->reply({{"error", "Can't cancel USSD."}});
	return 0;
}

int ModemService::apiSendCommand(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	int timeout = 0;
	std::string cmd;
	
	if (params["command"].is_string())
		cmd = params["command"];

	if (params["timeout"].is_number())
		timeout = params["timeout"];
	
	if (cmd.size() > 0) {
		auto [success, response] = m_modem->sendAtCommand(cmd, timeout);
		
		req->reply({
			{"success", success},
			{"response", response},
		});
		
		return 0;
	}
	
	return UBUS_STATUS_INVALID_ARGUMENT;
}

int ModemService::apiGetInfo(std::shared_ptr<UbusRequest> req) {
	auto levels = m_modem->getSignalLevels();
	auto ipv4 = m_modem->getIpInfo(4);
	auto ipv6 = m_modem->getIpInfo(6);
	auto op = m_modem->getOperator();
	
	double quality = 0;
	if (!std::isnan(levels.rssi_dbm))
		quality = rssiToPercent(levels.rssi_dbm, -100, -50);
	
	json response = {
		{"daemon", {
			{"uptime", getCurrentTimestamp() - m_start_time}
		}},
		{"modem", {
			{"vendor", m_modem->getVendor()},
			{"model", m_modem->getModel()},
			{"version", m_modem->getSwVersion()},
			{"imei", m_modem->getImei()},
		}},
		{"sim", {
			{"imsi", m_modem->getSimImsi()},
			{"number", m_modem->getSimNumber()},
		}},
		{"ipv4", {
			{"ip", ipv4.ip},
			{"mask", ipv4.mask},
			{"gw", ipv4.gw},
			{"dns1", ipv4.dns1},
			{"dns2", ipv4.dns2},
		}},
		{"ipv6", {
			{"ip", ipv6.ip},
			{"mask", ipv6.mask},
			{"gw", ipv6.gw},
			{"dns1", ipv6.dns1},
			{"dns2", ipv6.dns2},
		}},
		{"levels", {
			{"rssi_dbm", levels.rssi_dbm},
			{"bit_err_pct", levels.bit_err_pct},
			{"rscp_dbm", levels.rscp_dbm},
			{"ecio_db", levels.ecio_db},
			{"rsrq_db", levels.rsrq_db},
			{"rsrp_dbm", levels.rsrp_dbm},
			{"quality", quality}
		}},
		{"tech", {
			{"id", m_modem->getTech()},
			{"name", Modem::getTechName(m_modem->getTech())}
		}},
		{"operator", {
			{"id", op.id},
			{"name", op.name},
			{"registration", REG_STATUS_NAMES[op.reg]}
		}},
		{"network_status", {
			{"id", m_modem->getNetRegStatus()},
			{"name", Modem::getNetRegStatusName(m_modem->getNetRegStatus())}
		}}
	};
	req->reply(response);
	return 0;
}

int ModemService::apiReadSms(std::shared_ptr<UbusRequest> req) {
	static std::map<Modem::SmsStorage, std::string> storage_names = {
		{Modem::SMS_STORAGE_MT, "MT"},
		{Modem::SMS_STORAGE_ME, "ME"},
		{Modem::SMS_STORAGE_SM, "SM"},
	};
	
	auto &params = req->data();
	Modem::SmsDir dir = Modem::SMS_DIR_ALL;
	
	int dir_id = 4;
	if (params["dir"].is_number())
		dir = params["dir"].get<Modem::SmsDir>();
	
	req->defer();
	
	m_modem->getSmsList(dir, [=](bool status, std::vector<Modem::Sms> list) {
		if (!status) {
			req->reply({{"error", "Can't get SMS from modem."}});
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
		
		req->reply(response);
	});
	
	return 0;
}

int ModemService::apiDeleteSms(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	if (params["ids"].is_array() && params["ids"].size() > 0) {
		json response = {
			{"result", json::object()},
			{"errors", json::object()},
		};
		
		for (auto &id_item: params["ids"]) {
			if (!id_item.is_number())
				return UBUS_STATUS_INVALID_ARGUMENT;
		}
		
		for (auto &id_item: params["ids"]) {
			int id = id_item.get<int>();
			if (!m_modem->deleteSms(id)) {
				response["result"][std::to_string(id)] = false;
				response["errors"][std::to_string(id)] = strprintf("Message #%d failed to delete.", id);
			} else {
				response["result"][std::to_string(id)] = true;
			}
		}
		
		if (!response["errors"].size())
			response["errors"] = false;
		
		req->reply(response);
		
		return 0;
	}
	return UBUS_STATUS_INVALID_ARGUMENT;
}

int ModemService::apiSearchOperators(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	bool async = getBoolArg(params, "async", false);
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=]() {
		m_modem->searchOperators([=](bool status, std::vector<Modem::Operator> list) {
			json response = {
				{"list", json::array()}
			};
			for (auto &op: list) {
				json op_json = json::object();
				op_json["id"] = op.id;
				op_json["name"] = op.name;
				op_json["status"] = OPERATOR_STATUS_NAMES[op.status];
				op_json["tech"] = {
					{"id", op.tech},
					{"name", Modem::getTechName(op.tech)}
				};
				response["list"].push_back(op_json);
			}
			
			if (async) {
				setDeferredResult(deferred_id, response);
			} else {
				req->reply(response);
			}
		});
	}, 0);
	
	return 0;
}

int ModemService::apiSetOperator(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	Modem::NetworkTech tech = static_cast<Modem::NetworkTech>(getIntArg(params, "tech", Modem::TECH_UNKNOWN));
	std::string id = getStrArg(params, "id", "");
	bool async = getBoolArg(params, "async", false);
	
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=]() {
		json response = {};
		response["success"] = m_modem->setOperator(id, tech);
		
		if (async) {
			setDeferredResult(deferred_id, response);
		} else {
			req->reply(response);
		}
	}, 0);
	
	return 0;
}

int ModemService::apiGetDeferredResult(std::shared_ptr<UbusRequest> req) {
	json response = {};
	auto &params = req->data();
	std::string id = params["id"].is_string() ? params["id"] : "";
	
	auto it = m_deferred_results.find(id);
	if (it != m_deferred_results.end()) {
		response["result"] = it->second.result;
		response["ready"] = it->second.time > 0;
		response["exists"] = true;
		m_deferred_results.erase(id);
	} else {
		response["exists"] = false;
	}
	
	req->reply(response);
	
	return 0;
}

int ModemService::apiGetSettings(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	bool async = getBoolArg(params, "async", false);
	
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=]() {
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
	
	return 0;
}

int ModemService::apiSetNetworkMode(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	bool async = getBoolArg(params, "async", false);
	bool roaming = getBoolArg(params, "roaming", false);
	int mode_id = getIntArg(params, "mode", -1);
	
	std::string deferred_id = enableDefferedResult(req, async);
	
	Loop::setTimeout([=]() {
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
	
	return 0;
}

bool ModemService::runApi() {
	return m_ubus.object("usbmodem." + m_iface)
		.method("info", [=](auto req) {
			return apiGetInfo(req);
		})
		.method("get_settings", [=](auto req) {
			return apiGetSettings(req);
		})
		.method("send_command", [=](auto req) {
			return apiSendCommand(req);
		}, {
			{"command", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.method("send_ussd", [=](auto req) {
			return apiSendUssd(req);
		}, {
			{"query", UbusObject::STRING},
			{"answer", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.method("cancel_ussd", [=](auto req) {
			return apiCancelUssd(req);
		})
		.method("search_operators", [=](auto req) {
			return apiSearchOperators(req);
		})
		.method("set_operator", [=](auto req) {
			return apiSetOperator(req);
		}, {
			{"id", UbusObject::STRING},
			{"tech", UbusObject::INT32}
		})
		.method("read_sms", [=](auto req) {
			return apiReadSms(req);
		}, {
			{"dir", UbusObject::STRING}
		})
		.method("delete_sms", [=](auto req) {
			return apiDeleteSms(req);
		}, {
			{"ids", UbusObject::ARRAY}
		})
		.method("get_deferred_result", [=](auto req) {
			return apiGetDeferredResult(req);
		})
		.method("set_network_mode", [=](auto req) {
			return apiSetNetworkMode(req);
		})
		.attach();
}
