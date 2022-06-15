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

static std::vector<Modem::NetworkMode> ALL_NETWORK_MODES = {
	Modem::NET_MODE_AUTO,
	Modem::NET_MODE_ONLY_2G,
	Modem::NET_MODE_ONLY_3G,
	Modem::NET_MODE_ONLY_4G,
	Modem::NET_MODE_PREFER_2G,
	Modem::NET_MODE_PREFER_3G,
	Modem::NET_MODE_PREFER_4G,
	Modem::NET_MODE_2G_3G_AUTO,
	Modem::NET_MODE_2G_3G_PREFER_2G,
	Modem::NET_MODE_2G_3G_PREFER_3G,
	Modem::NET_MODE_2G_4G_AUTO,
	Modem::NET_MODE_2G_4G_PREFER_2G,
	Modem::NET_MODE_2G_4G_PREFER_4G,
	Modem::NET_MODE_3G_4G_AUTO,
	Modem::NET_MODE_3G_4G_PREFER_3G,
	Modem::NET_MODE_3G_4G_PREFER_4G,
};

void ModemServiceApi::apiGetModemInfo(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=]() {
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
			{"imei", modem_info.imei},
			{"capabilities", m_modem->getCapabilities()}
		});
	}, 0);
}

void ModemServiceApi::apiGetSimInfo(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=]() {
		auto [success, sim_info] = m_modem->getSimInfo();
		if (!success) {
			reply(req, {
				{"error", "Can't get sim info"}
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
	Loop::setTimeout([=]() {
		auto [success, net_info] = m_modem->getNetworkInfo();
		if (!success) {
			reply(req, {
				{"error", "Can't get network info"}
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
				{"main_rsrq_db", net_info.signal.main_rsrq_db},
				{"main_rsrp_dbm", net_info.signal.main_rsrp_dbm},
				{"div_rsrq_db", net_info.signal.div_rsrq_db},
				{"div_rsrp_dbm", net_info.signal.div_rsrp_dbm},
				{"quality", quality}
			}},
			{"cell", {
				{"cell_id", net_info.cell.cell_id},
				{"loc_id", net_info.cell.loc_id},
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
	
	Loop::setTimeout([=]() {
		if (!is_answer && m_modem->isUssdWaitReply())
			m_modem->cancelUssd();
		
		bool success = m_modem->sendUssd(query, [=](Modem::UssdCode code, const std::string &response) {
			if (code == Modem::USSD_ERROR) {
				reply(req, {{"error", response}});
			} else {
				reply(req, {
					{"code", code},
					{"response", response}
				});
			}
		}, timeout);
		
		if (!success)
			reply(req, {{"error", "Can't send USSD."}});
	}, 0);
}

void ModemServiceApi::apiCancelUssd(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=]() {
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
	
	if (!cmd.size()) {
		reply(req, {}, UBUS_STATUS_INVALID_ARGUMENT);
		return;
	}
	
	Loop::setTimeout([=]() {
		auto [success, response] = m_modem->sendAtCommand(cmd, timeout);
		reply(req, {
			{"success", success},
			{"response", response},
		});
	}, 0);
}

void ModemServiceApi::apiReadSms(std::shared_ptr<UbusRequest> req) {
	static const std::map<std::string, SmsDb::SmsType> sms_types = {
		{"incoming", SmsDb::SMS_INCOMING},
		{"outgoing", SmsDb::SMS_OUTGOING},
		{"draft", SmsDb::SMS_DRAFT},
	};
	
	const auto &params = req->data();
	int offset = getIntArg(params, "offset", 0);
	int limit = getIntArg(params, "limit", 100);
	std::string type_name = getStrArg(params, "type", "incoming");
	SmsDb::SmsType type = sms_types.find(type_name) != sms_types.end() ? sms_types.at(type_name) : SmsDb::SMS_INCOMING;
	
	Loop::setTimeout([=]() {
		auto list = m_sms->getSmsList(type, offset, limit);
		
		json response = {
			{"capacity", {
				{"used", m_sms->getUsedCapacity()},
				{"total", m_sms->getMaxCapacity()}
			}},
			{"counters", {
				{"incoming", m_sms->getSmsCount(SmsDb::SMS_INCOMING)},
				{"outgoing", m_sms->getSmsCount(SmsDb::SMS_OUTGOING)},
				{"draft", m_sms->getSmsCount(SmsDb::SMS_DRAFT)}
			}},
			{"storage", m_sms->getStorageTypeName()},
			{"messages", json::array()}
		};
		
		for (auto &sms: list) {
			json message = {
				{"id", sms.id},
				{"addr", sms.addr},
				{"smsc", sms.smsc},
				{"time", sms.time},
				{"unread", (sms.flags & SmsDb::SMS_IS_UNREAD) != 0},
				{"invalid", (sms.flags & SmsDb::SMS_IS_INVALID) != 0},
				{"parts", json::array()}
			};
			
			for (auto &part: sms.parts) {
				if (part.text.size() > 0) {
					message["parts"].push_back(part.text);
				} else {
					message["parts"].push_back(nullptr);
				}
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
	
	Loop::setTimeout([=]() {
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
	Loop::setTimeout([=]() {
		auto [success, list] = m_modem->searchOperators();
		if (!success) {
			reply(req, {
				{"error", "Search operators failed"}
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
	
	Loop::setTimeout([=]() {
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

void ModemServiceApi::apiGetNetworkSettings(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=]() {
		json response = {
			{"network_modes", json::array()}
		};
		
		auto [success, list] = m_modem->getNetworkModes();
		if (!success) {
			reply(req, {{"error", "getNetworkModes error"}});
			return;
		}
		
		auto [success2, curr_mode] = m_modem->getCurrentNetworkMode();
		if (!success2) {
			reply(req, {{"error", "getCurrentNetworkMode error"}});
			return;
		}
		
		auto [success3, roaming] = m_modem->isRoamingEnabled();
		if (!success3) {
			reply(req, {{"error", "isRoamingEnabled error"}});
			return;
		}
		
		for (auto &mode: list)
			response["network_modes"].push_back(m_modem->getEnumName(mode));
		
		response["roaming"] = roaming;
		response["network_mode"] = m_modem->getEnumName(curr_mode);
		
		reply(req, response);
	}, 0);
}

void ModemServiceApi::apiSetNetworkSettings(std::shared_ptr<UbusRequest> req) {
	const auto &params = req->data();
	std::string mode_name = getStrArg(params, "mode", "");
	bool roaming = getBoolArg(params, "roaming", false);
	
	Modem::NetworkMode mode = Modem::NET_MODE_UNKNOWN;
	for (auto v: ALL_NETWORK_MODES) {
		if (strcasecmp(Modem::getEnumName(v), mode_name.c_str()) == 0) {
			mode = v;
			break;
		}
	}
	
	Loop::setTimeout([=]() {
		json response = {{"success", true}};
		
		if (!m_modem->setNetworkMode(mode)) {
			response["success"] = false;
			response["error"] = "Can't set network mode";
		} else if (!m_modem->setDataRoaming(roaming)) {
			response["success"] = false;
			response["error"] = "Can't set data roaming";
		}
		
		reply(req, response);
	}, 0);
}


void ModemServiceApi::apiGetNeighboringCell(std::shared_ptr<UbusRequest> req) {
	Loop::setTimeout([=]() {
		auto [success, list] = m_modem->getNeighboringCell();
		if (!success) {
			reply(req, {{"error", "getNeighboringCell error"}});
			return;
		}
		
		json response = {{"list", json::array()}};
		for (auto &cell: list) {
			response["list"].push_back({
				{"rssi_dbm", cell.rssi_dbm},
				{"rscp_dbm", cell.rscp_dbm},
				{"mcc", cell.mcc},
				{"mnc", cell.mnc},
				{"loc_id", cell.loc_id},
				{"cell_id", cell.cell_id},
				{"freq", cell.freq},
			});
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

void ModemServiceApi::reply(std::shared_ptr<UbusRequest> req, json result, int status) {
	if (m_deferred_results.find(req->uniqKey()) != m_deferred_results.end()) {
		m_deferred_results[req->uniqKey()].time = getCurrentTimestamp();
		m_deferred_results[req->uniqKey()].result = result;
		m_deferred_results[req->uniqKey()].status = status;
	} else {
		UbusLoop::setTimeout([=]() {
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
		
		.method("sendCommand", [this](auto req) {
			initApiRequest(req);
			apiSendCommand(req);
			return 0;
		}, {
			{"command", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		
		.method("sendUssd", [this](auto req) {
			initApiRequest(req);
			apiSendUssd(req);
			return 0;
		}, {
			{"query", UbusObject::STRING},
			{"answer", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		
		.method("readSms", [this](auto req) {
			initApiRequest(req);
			apiReadSms(req);
			return 0;
		}, {
			{"dir", UbusObject::STRING}
		})
		
		.method("deleteSms", [this](auto req) {
			initApiRequest(req);
			apiDeleteSms(req);
			return 0;
		}, {
			{"ids", UbusObject::ARRAY}
		})
		
		.method("cancelUssd", [this](auto req) {
			initApiRequest(req);
			apiCancelUssd(req);
			return 0;
		})
		
		.method("getNetworkSettings", [this](auto req) {
			initApiRequest(req);
			apiGetNetworkSettings(req);
			return 0;
		})
		
		.method("setNetworkSettings", [this](auto req) {
			initApiRequest(req);
			apiSetNetworkSettings(req);
			return 0;
		})
		
		.method("getNeighboringCell", [this](auto req) {
			initApiRequest(req);
			apiGetNeighboringCell(req);
			return 0;
		})
		
		.attach();
}
