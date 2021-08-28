#include "ModemService.h"
#include "AtChannel.h"

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
			{"eclo_db", levels.eclo_db},
			{"rsrq_db", levels.rsrq_db},
			{"rsrp_dbm", levels.rsrp_dbm},
		}},
		{"tech", {
			{"id", m_modem->getTech()},
			{"name", Modem::getTechName(m_modem->getTech())}
		}},
		{"network_status", {
			{"id", m_modem->getNetRegStatus()},
			{"name", Modem::getNetRegStatusName(m_modem->getNetRegStatus())}
		}}
	};
	req->reply(response);
	return 0;
}

bool ModemService::runApi() {
	return m_ubus.object("usbmodem." + m_iface)
		.method("info", [=](auto req) {
			return apiGetInfo(req);
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
		.attach();
}
