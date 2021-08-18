#include "ModemService.h"
#include "AtChannel.h"

int ModemService::apiSendUssd(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	int timeout = 0;
	std::string query;
	
	if (params["query"].is_string())
		query = params["query"];
	
	if (params["timeout"].is_number())
		timeout = params["timeout"];
	
	req->defer();
	
	m_modem->sendUssd(query, [=](int code, const std::string &response) {
		req->reply({
			{"code", code},
			{"response", response}
		});
	});
	
	return 0;
}

int ModemService::apiSendCommand(std::shared_ptr<UbusRequest> req) {
	auto &params = req->data();
	
	int timeout = 0;
	std::string cmd, prefix, response;
	
	if (params["command"].is_string())
		cmd = params["command"];
	
	if (params["prefix"].is_string())
		prefix = params["prefix"];
	
	if (params["response"].is_string())
		response = params["response"];
	
	if (params["timeout"].is_number())
		timeout = params["timeout"];
	
	if (cmd.size() > 0) {
		json response;
		AtChannel::Response cmd_response;
		AtChannel *at_channel = m_modem->getAtChannel();
		
		if (params["response"] == "numeric") {
			cmd_response = at_channel->sendCommandNumeric(cmd, timeout);
		} else if (params["response"] == "multiline") {
			cmd_response = at_channel->sendCommandMultiline(cmd, prefix, timeout);
		} else if (params["response"] == "dial") {
			cmd_response = at_channel->sendCommandDial(cmd, timeout);
		} else {
			if (prefix == "") {
				cmd_response = at_channel->sendCommandNoPrefix(cmd, timeout);
			} else {
				cmd_response = at_channel->sendCommand(cmd, prefix, timeout);
			}
		}
		
		req->reply({
			{"error", cmd_response.error},
			{"status", cmd_response.status},
			{"lines", cmd_response.lines}
		});
		
		return 0;
	} else {
		return UBUS_STATUS_INVALID_ARGUMENT;
	}
}

int ModemService::apiGetInfo(std::shared_ptr<UbusRequest> req) {
	Modem::SignalLevels levels;
	IpInfo ipv4, ipv6;
	
	m_modem->getSignalLevels(&levels);
	m_modem->getIpInfo(4, &ipv4);
	m_modem->getIpInfo(6, &ipv6);
	
	json response = {
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
		{"daemon", {
			{"uptime", getCurrentTimestamp() - m_start_time}
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
			{"response", UbusObject::STRING},
			{"prefix", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.method("send_ussd", [=](auto req) {
			return apiSendUssd(req);
		}, {
			{"query", UbusObject::STRING},
			{"timeout", UbusObject::INT32}
		})
		.attach();
}
